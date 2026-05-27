#include "lsp_server.hpp"

#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <istream>
#include <ostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace dune::lsp {

namespace {

std::string json_escape(const std::string& value) {
    std::string escaped;
    for (const unsigned char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (character < 0x20) {
                escaped += "\\u00";
                constexpr char digits[] = "0123456789abcdef";
                escaped += digits[character >> 4];
                escaped += digits[character & 0x0f];
            } else {
                escaped += static_cast<char>(character);
            }
            break;
        }
    }

    return escaped;
}

int hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::string percent_decode(std::string value) {
    std::string decoded;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size()) {
            const int high = hex_value(value[index + 1]);
            const int low = hex_value(value[index + 2]);
            if (high >= 0 && low >= 0) {
                decoded += static_cast<char>((high << 4) | low);
                index += 2;
                continue;
            }
        }

        decoded += value[index];
    }

    return decoded;
}

std::filesystem::path path_from_uri(const std::string& uri) {
    constexpr std::string_view file_scheme = "file://";
    if (!uri.starts_with(file_scheme)) {
        return {};
    }

    std::string path = percent_decode(uri.substr(file_scheme.size()));
#if defined(_WIN32)
    if (path.size() >= 3 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':') {
        path.erase(path.begin());
    }
#endif
    return std::filesystem::path(path);
}

std::filesystem::path source_directory_for(const std::string& uri, const std::filesystem::path& fallback) {
    if (!fallback.empty()) {
        return fallback;
    }

    const std::filesystem::path path = path_from_uri(uri);
    if (!path.empty()) {
        return path.parent_path();
    }

    return std::filesystem::current_path();
}

Diagnostic diagnostic_from_error(const std::string& message) {
    static const std::regex range_pattern(R"(^line ([0-9]+), columns ([0-9]+)-([0-9]+): (.*)$)");
    static const std::regex single_pattern(R"(^line ([0-9]+), column ([0-9]+): (.*)$)");

    std::smatch match;
    if (std::regex_match(message, match, range_pattern)) {
        return Diagnostic{static_cast<std::size_t>(std::stoull(match[1].str())),
                          static_cast<std::size_t>(std::stoull(match[2].str())),
                          static_cast<std::size_t>(std::stoull(match[3].str())), match[4].str()};
    }

    if (std::regex_match(message, match, single_pattern)) {
        const std::size_t column = static_cast<std::size_t>(std::stoull(match[2].str()));
        return Diagnostic{static_cast<std::size_t>(std::stoull(match[1].str())), column, column, match[3].str()};
    }

    return Diagnostic{1, 1, 1, message};
}

std::string find_json_string(const std::string& json, const std::string& key, std::size_t offset = 0) {
    const std::string wanted = "\"" + key + "\"";
    const std::size_t key_pos = json.find(wanted, offset);
    if (key_pos == std::string::npos) {
        return {};
    }

    std::size_t pos = json.find(':', key_pos + wanted.size());
    if (pos == std::string::npos) {
        return {};
    }

    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    if (pos >= json.size() || json[pos] != '"') {
        return {};
    }

    ++pos;
    std::string value;
    while (pos < json.size()) {
        const char current = json[pos++];
        if (current == '"') {
            return value;
        }

        if (current != '\\') {
            value += current;
            continue;
        }

        if (pos >= json.size()) {
            return value;
        }

        const char escaped = json[pos++];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            value += escaped;
            break;
        case 'b':
            value += '\b';
            break;
        case 'f':
            value += '\f';
            break;
        case 'n':
            value += '\n';
            break;
        case 'r':
            value += '\r';
            break;
        case 't':
            value += '\t';
            break;
        case 'u':
            if (pos + 3 < json.size()) {
                int code = 0;
                bool valid = true;
                for (int index = 0; index < 4; ++index) {
                    const int digit = hex_value(json[pos + index]);
                    if (digit < 0) {
                        valid = false;
                        break;
                    }
                    code = (code << 4) | digit;
                }
                pos += 4;
                value += valid && code < 128 ? static_cast<char>(code) : '?';
            }
            break;
        default:
            value += escaped;
            break;
        }
    }

    return value;
}

std::string find_raw_id(const std::string& json) {
    const std::string wanted = "\"id\"";
    const std::size_t key_pos = json.find(wanted);
    if (key_pos == std::string::npos) {
        return {};
    }

    std::size_t pos = json.find(':', key_pos + wanted.size());
    if (pos == std::string::npos) {
        return {};
    }

    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    if (pos >= json.size()) {
        return {};
    }

    if (json[pos] == '"') {
        std::string value = "\"";
        ++pos;
        bool escaped = false;
        while (pos < json.size()) {
            const char current = json[pos++];
            value += current;
            if (escaped) {
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            } else if (current == '"') {
                return value;
            }
        }
        return value;
    }

    const std::size_t start = pos;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' &&
           !std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    return json.substr(start, pos - start);
}

std::string find_change_text(const std::string& json) {
    const std::size_t changes = json.find("\"contentChanges\"");
    if (changes == std::string::npos) {
        return {};
    }

    return find_json_string(json, "text", changes);
}

void write_message(std::ostream& output, const std::string& body) {
    output << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    output.flush();
}

std::string diagnostics_json(const std::vector<Diagnostic>& diagnostics) {
    std::string json = "[";
    for (std::size_t index = 0; index < diagnostics.size(); ++index) {
        const Diagnostic& diagnostic = diagnostics[index];
        if (index > 0) {
            json += ",";
        }

        const std::size_t line = diagnostic.line == 0 ? 0 : diagnostic.line - 1;
        const std::size_t start = diagnostic.start_column == 0 ? 0 : diagnostic.start_column - 1;
        const std::size_t end = std::max(diagnostic.end_column, diagnostic.start_column);

        json += "{\"range\":{\"start\":{\"line\":" + std::to_string(line) + ",\"character\":" + std::to_string(start) +
                "},\"end\":{\"line\":" + std::to_string(line) + ",\"character\":" + std::to_string(end) +
                "}},\"severity\":1,\"source\":\"dune\",\"message\":\"" + json_escape(diagnostic.message) + "\"}";
    }

    json += "]";
    return json;
}

void publish_diagnostics(std::ostream& output, const std::string& uri, const std::vector<Diagnostic>& diagnostics) {
    write_message(output, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"" +
                              json_escape(uri) + "\",\"diagnostics\":" + diagnostics_json(diagnostics) + "}}");
}

bool read_message(std::istream& input, std::string& body) {
    std::string line;
    std::size_t content_length = 0;
    bool saw_header = false;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            break;
        }

        constexpr std::string_view header = "Content-Length:";
        if (line.starts_with(header)) {
            content_length = static_cast<std::size_t>(std::stoull(line.substr(header.size())));
            saw_header = true;
        }
    }

    if (!saw_header || content_length == 0 || !input) {
        return false;
    }

    body.assign(content_length, '\0');
    input.read(body.data(), static_cast<std::streamsize>(content_length));
    return input.gcount() == static_cast<std::streamsize>(content_length);
}

} // namespace

std::vector<Diagnostic> diagnose_source(const std::string& source, const std::string& uri,
                                        const std::filesystem::path& source_directory) {
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        ModuleLoader loader;
        TypeChecker checker;
        checker.check(loader.resolve(parser.parse(), source_directory_for(uri, source_directory)));
        return {};
    } catch (const std::exception& error) {
        return {diagnostic_from_error(error.what())};
    }
}

int run(std::istream& input, std::ostream& output) {
    std::unordered_map<std::string, std::string> documents;

    std::string message;
    while (read_message(input, message)) {
        const std::string method = find_json_string(message, "method");
        const std::string id = find_raw_id(message);

        if (method == "initialize") {
            const std::string response_id = id.empty() ? "null" : id;
            write_message(output,
                          "{\"jsonrpc\":\"2.0\",\"id\":" + response_id +
                              ",\"result\":{\"capabilities\":{\"textDocumentSync\":1},\"serverInfo\":{\"name\":\"dune-"
                              "lsp\",\"version\":\"0.1.0\"}}}");
            continue;
        }

        if (method == "shutdown") {
            const std::string response_id = id.empty() ? "null" : id;
            write_message(output, "{\"jsonrpc\":\"2.0\",\"id\":" + response_id + ",\"result\":null}");
            continue;
        }

        if (method == "exit") {
            break;
        }

        if (method == "textDocument/didOpen") {
            const std::string uri = find_json_string(message, "uri");
            const std::string text = find_json_string(message, "text");
            documents[uri] = text;
            publish_diagnostics(output, uri, diagnose_source(text, uri));
            continue;
        }

        if (method == "textDocument/didChange") {
            const std::string uri = find_json_string(message, "uri");
            const std::string text = find_change_text(message);
            documents[uri] = text;
            publish_diagnostics(output, uri, diagnose_source(text, uri));
            continue;
        }

        if (method == "textDocument/didClose") {
            const std::string uri = find_json_string(message, "uri");
            documents.erase(uri);
            publish_diagnostics(output, uri, {});
        }
    }

    return 0;
}

} // namespace dune::lsp
