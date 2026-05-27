#include "lsp_server.hpp"

#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <istream>
#include <optional>
#include <ostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace dune::lsp {

namespace {

#ifndef DUNE_STDLIB_PATH
#define DUNE_STDLIB_PATH "stdlib"
#endif

constexpr std::size_t completion_kind_method = 2;
constexpr std::size_t completion_kind_function = 3;
constexpr std::size_t completion_kind_variable = 6;
constexpr std::size_t completion_kind_module = 9;
constexpr std::size_t completion_kind_enum = 13;
constexpr std::size_t completion_kind_keyword = 14;
constexpr std::size_t completion_kind_enum_member = 20;
constexpr std::size_t completion_kind_constant = 21;
constexpr std::size_t completion_kind_struct = 22;
constexpr std::size_t completion_kind_type_parameter = 25;

struct SourcePosition {
    std::size_t line = 0;
    std::size_t character = 0;
};

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

std::optional<std::size_t> find_json_unsigned(const std::string& json, const std::string& key, std::size_t offset = 0) {
    const std::string wanted = "\"" + key + "\"";
    const std::size_t key_pos = json.find(wanted, offset);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t pos = json.find(':', key_pos + wanted.size());
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    const std::size_t start = pos;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }

    if (start == pos) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(std::stoull(json.substr(start, pos - start)));
}

std::optional<SourcePosition> find_position(const std::string& json) {
    const std::size_t position = json.find("\"position\"");
    if (position == std::string::npos) {
        return std::nullopt;
    }

    std::optional<std::size_t> line = find_json_unsigned(json, "line", position);
    std::optional<std::size_t> character = find_json_unsigned(json, "character", position);
    if (!line.has_value() || !character.has_value()) {
        return std::nullopt;
    }

    return SourcePosition{*line, *character};
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

bool has_completion(const std::vector<CompletionItem>& completions, const std::string& label) {
    return std::ranges::any_of(completions, [&label](const CompletionItem& item) { return item.label == label; });
}

void add_completion(std::vector<CompletionItem>& completions, std::string label, std::string detail, std::size_t kind) {
    if (label.empty() || has_completion(completions, label)) {
        return;
    }

    completions.push_back(CompletionItem{std::move(label), std::move(detail), kind});
}

void add_static_completions(std::vector<CompletionItem>& completions) {
    for (const std::string_view keyword :
         {"as",   "break",  "const", "continue", "else",  "enum",   "export", "extern", "fn",   "for",  "if",
          "impl", "import", "let",   "match",    "print", "return", "struct", "while",  "true", "false"}) {
        add_completion(completions, std::string(keyword), "keyword", completion_kind_keyword);
    }

    for (const std::string_view type :
         {"int",   "bool",  "i8",     "i16",    "i32",    "i64",  "isize",  "u8",     "u16",   "u32",  "u64",
          "usize", "uint8", "uint16", "uint32", "uint64", "real", "real32", "real64", "glyph", "text", "unit"}) {
        add_completion(completions, std::string(type), "type", completion_kind_keyword);
    }

    for (const std::string_view bound : {"integer", "numeric", "comparable", "ordered"}) {
        add_completion(completions, std::string(bound), "generic bound", completion_kind_type_parameter);
    }
}

std::string completion_items_json(const std::vector<CompletionItem>& completions) {
    std::string json = "{\"isIncomplete\":false,\"items\":[";
    for (std::size_t index = 0; index < completions.size(); ++index) {
        const CompletionItem& item = completions[index];
        if (index > 0) {
            json += ",";
        }

        json += "{\"label\":\"" + json_escape(item.label) + "\",\"kind\":" + std::to_string(item.kind) +
                ",\"detail\":\"" + json_escape(item.detail) + "\"}";
    }

    json += "]}";
    return json;
}

std::string hover_json(const Hover& hover) {
    return "{\"contents\":{\"kind\":\"markdown\",\"value\":\"" + json_escape(hover.contents) + "\"}}";
}

void publish_diagnostics(std::ostream& output, const std::string& uri, const std::vector<Diagnostic>& diagnostics) {
    write_message(output, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"" +
                              json_escape(uri) + "\",\"diagnostics\":" + diagnostics_json(diagnostics) + "}}");
}

bool is_identifier_like(const Token& token) {
    return token.type == TokenType::identifier || token.type == TokenType::text_keyword;
}

std::vector<Token> tokenize_best_effort(const std::string& source) {
    try {
        Lexer lexer(source);
        return lexer.tokenize();
    } catch (const std::exception&) {
        return {};
    }
}

std::vector<std::string> imports_in_source(const std::string& source) {
    const std::vector<Token> tokens = tokenize_best_effort(source);
    std::vector<std::string> imports;
    for (std::size_t index = 0; index + 1 < tokens.size(); ++index) {
        if (tokens[index].type == TokenType::import_keyword && is_identifier_like(tokens[index + 1])) {
            imports.push_back(tokens[index + 1].lexeme);
        }
    }

    return imports;
}

void add_token_symbols(const std::string& source, std::vector<CompletionItem>& completions) {
    const std::vector<Token> tokens = tokenize_best_effort(source);
    for (std::size_t index = 0; index + 1 < tokens.size(); ++index) {
        if (tokens[index].type == TokenType::fn_keyword && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "function", completion_kind_function);
        }

        if (tokens[index].type == TokenType::let && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "variable", completion_kind_variable);
        }

        if (tokens[index].type == TokenType::const_keyword && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "constant", completion_kind_constant);
        }

        if (tokens[index].type == TokenType::struct_keyword && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "struct", completion_kind_struct);
        }

        if (tokens[index].type == TokenType::enum_keyword && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "enum", completion_kind_enum);
        }
    }
}

void add_search_path(std::vector<std::filesystem::path>& paths, const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }

    const std::string value = path.string();
    const bool exists = std::ranges::any_of(
        paths, [&value](const std::filesystem::path& current) { return current.string() == value; });
    if (!exists) {
        paths.push_back(path);
    }
}

std::vector<std::filesystem::path> module_search_paths(const std::filesystem::path& source_directory) {
    std::vector<std::filesystem::path> paths;
    add_search_path(paths, source_directory);

    const char* env_path = std::getenv("DUNE_STDLIB_PATH");
    if (env_path != nullptr && *env_path != '\0') {
#if defined(_WIN32)
        constexpr char delimiter = ';';
#else
        constexpr char delimiter = ':';
#endif
        std::stringstream stream(env_path);
        std::string item;
        while (std::getline(stream, item, delimiter)) {
            add_search_path(paths, item);
        }
    }

    add_search_path(paths, DUNE_STDLIB_PATH);
    add_search_path(paths, std::filesystem::current_path());
    return paths;
}

std::optional<std::filesystem::path> find_module_file(const std::string& module_name,
                                                      const std::filesystem::path& source_directory) {
    if (module_name.empty() || module_name.find("..") != std::string::npos) {
        return std::nullopt;
    }

    const std::filesystem::path module_path = std::filesystem::path(module_name).replace_extension(".dn");
    for (const std::filesystem::path& search_path : module_search_paths(source_directory)) {
        std::filesystem::path candidate = search_path / module_path;
        std::error_code error;
        if (std::filesystem::exists(candidate, error)) {
            return candidate;
        }
    }

    return std::nullopt;
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<Program> parse_program_best_effort(const std::string& source) {
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        return parser.parse();
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool module_has_explicit_exports(const Program& program) {
    return std::ranges::any_of(program.statements, [](const Statement& statement) {
        return statement.exported &&
               (statement.kind == StatementKind::function || statement.kind == StatementKind::const_statement ||
                statement.kind == StatementKind::struct_statement || statement.kind == StatementKind::enum_statement ||
                statement.kind == StatementKind::impl_statement);
    });
}

void add_enum_variants(const Statement& statement, bool visible, std::vector<CompletionItem>& completions) {
    if (!visible || statement.kind != StatementKind::enum_statement) {
        return;
    }

    for (const Parameter& variant : statement.parameters) {
        add_completion(completions, variant.name, "enum variant", completion_kind_enum_member);
    }
}

void add_module_members(const Program& program, std::vector<CompletionItem>& completions) {
    const bool exported_only = module_has_explicit_exports(program);
    for (const Statement& statement : program.statements) {
        const bool visible = !exported_only || statement.exported;
        if (statement.kind == StatementKind::function && visible) {
            add_completion(completions, statement.name, "function", completion_kind_function);
        }

        if (statement.kind == StatementKind::const_statement && visible) {
            add_completion(completions, statement.name, "constant", completion_kind_constant);
        }

        if (statement.kind == StatementKind::struct_statement && visible) {
            add_completion(completions, statement.name, "struct", completion_kind_struct);
        }

        if (statement.kind == StatementKind::enum_statement && visible) {
            add_completion(completions, statement.name, "enum", completion_kind_enum);
            add_enum_variants(statement, true, completions);
        }

        if (statement.kind == StatementKind::impl_statement) {
            for (const Statement& method : statement.body) {
                const bool method_visible = !exported_only || statement.exported || method.exported;
                if (method.kind == StatementKind::function && method_visible) {
                    add_completion(completions, method.name, "method", completion_kind_method);
                }
            }
        }
    }
}

void add_module_names(const std::filesystem::path& source_directory, std::vector<CompletionItem>& completions) {
    for (const std::filesystem::path& search_path : module_search_paths(source_directory)) {
        std::error_code error;
        if (!std::filesystem::is_directory(search_path, error)) {
            continue;
        }

        for (std::filesystem::directory_iterator iterator(search_path, error), end; !error && iterator != end;
             iterator.increment(error)) {
            if (iterator->path().extension() == ".dn") {
                add_completion(completions, iterator->path().stem().string(), "module", completion_kind_module);
            }
        }
    }
}

void add_imported_module_completions(const std::vector<std::string>& imports,
                                     std::vector<CompletionItem>& completions) {
    for (const std::string& module : imports) {
        add_completion(completions, module, "module", completion_kind_module);
    }
}

void add_receiver_method_completions(std::vector<CompletionItem>& completions) {
    for (const std::string_view method :
         {"len",     "push",  "pop",    "clear",  "is_empty", "contains",   "starts_with", "ends_with",
          "char_at", "slice", "prefix", "suffix", "trim",     "trim_start", "trim_end",    "copy",
          "reverse", "first", "last",   "append", "index_of", "count"}) {
        add_completion(completions, std::string(method), "method", completion_kind_method);
    }
}

bool is_identifier_character(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) || value == '_';
}

std::size_t offset_at_position(const std::string& source, SourcePosition position) {
    std::size_t line = 0;
    std::size_t line_start = 0;
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (line == position.line) {
            std::size_t line_end = index;
            while (line_end < source.size() && source[line_end] != '\n') {
                ++line_end;
            }
            return std::min(line_start + position.character, line_end);
        }

        if (source[index] == '\n') {
            ++line;
            line_start = index + 1;
        }
    }

    if (line == position.line) {
        return std::min(line_start + position.character, source.size());
    }

    return source.size();
}

std::optional<std::string> qualifier_before_cursor(const std::string& source, SourcePosition position) {
    const std::size_t offset = offset_at_position(source, position);
    std::size_t cursor = std::min(offset, source.size());
    while (cursor > 0 && is_identifier_character(source[cursor - 1])) {
        --cursor;
    }

    if (cursor == 0 || source[cursor - 1] != '.') {
        return std::nullopt;
    }

    std::size_t end = cursor - 1;
    std::size_t start = end;
    while (start > 0 && is_identifier_character(source[start - 1])) {
        --start;
    }

    if (start == end) {
        return std::nullopt;
    }

    return source.substr(start, end - start);
}

std::optional<std::size_t> token_index_at_position(const std::vector<Token>& tokens, SourcePosition position) {
    const std::size_t target_line = position.line + 1;
    const std::size_t target_column = position.character + 1;

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const Token& token = tokens[index];
        if (token.lexeme.empty() || token.type == TokenType::eof || token.line != target_line) {
            continue;
        }

        const std::size_t start = token.column;
        const std::size_t end = token.column + token.lexeme.size();
        if (target_column >= start && target_column <= end) {
            return index;
        }
    }

    return std::nullopt;
}

std::string type_annotation_name(const TypeAnnotation& annotation, std::string_view fallback = "int") {
    if (!annotation.has_type) {
        return std::string(fallback);
    }

    return type_name(annotation.type);
}

std::string generic_parameters_text(const std::vector<GenericParameter>& parameters) {
    if (parameters.empty()) {
        return {};
    }

    std::string text = "<";
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += parameters[index].name;
        if (!parameters[index].bound.empty()) {
            text += ": ";
            text += parameters[index].bound;
        }
    }

    text += ">";
    return text;
}

std::string parameter_list_text(const std::vector<Parameter>& parameters) {
    std::string text = "(";
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        if (index > 0) {
            text += ", ";
        }
        text += parameters[index].name + ": " + type_annotation_name(parameters[index].type);
    }

    text += ")";
    return text;
}

std::string function_signature(const Statement& statement) {
    std::string signature = "fn " + statement.name + generic_parameters_text(statement.generic_parameters) +
                            parameter_list_text(statement.parameters);
    if (statement.type.has_type || statement.kind == StatementKind::function) {
        signature += " -> " + type_annotation_name(statement.type, "unit");
    }
    return signature;
}

std::optional<std::string> literal_expression_type(const Expression& expression) {
    switch (expression.kind) {
    case ExpressionKind::number:
        return "int";
    case ExpressionKind::floating:
        return "real";
    case ExpressionKind::character:
        return "glyph";
    case ExpressionKind::string:
        return "text";
    case ExpressionKind::boolean:
        return "bool";
    case ExpressionKind::array:
        return "[unknown]";
    case ExpressionKind::struct_literal:
        return expression.lexeme;
    case ExpressionKind::identifier:
    case ExpressionKind::index:
    case ExpressionKind::slice:
    case ExpressionKind::member:
    case ExpressionKind::unary:
    case ExpressionKind::cast:
    case ExpressionKind::binary:
    case ExpressionKind::match_expression:
    case ExpressionKind::call:
    case ExpressionKind::method_call:
        return std::nullopt;
    }

    return std::nullopt;
}

std::string variable_type_text(const Statement& statement) {
    if (statement.type.has_type) {
        return type_name(statement.type.type);
    }

    if (statement.expression != nullptr) {
        if (std::optional<std::string> inferred = literal_expression_type(*statement.expression)) {
            return *inferred;
        }
    }

    return "unknown";
}

std::string code_hover(std::string declaration) {
    return "```dune\n" + std::move(declaration) + "\n```";
}

std::string declaration_hover(const Statement& statement) {
    switch (statement.kind) {
    case StatementKind::let:
        return code_hover("let " + statement.name + ": " + variable_type_text(statement));
    case StatementKind::const_statement:
        return code_hover("const " + statement.name + ": " + variable_type_text(statement));
    case StatementKind::function:
        return code_hover(function_signature(statement));
    case StatementKind::struct_statement:
        return code_hover("struct " + statement.name + generic_parameters_text(statement.generic_parameters));
    case StatementKind::enum_statement:
        return code_hover("enum " + statement.name + generic_parameters_text(statement.generic_parameters));
    case StatementKind::impl_statement:
    case StatementKind::assign:
    case StatementKind::print:
    case StatementKind::block:
    case StatementKind::if_statement:
    case StatementKind::while_statement:
    case StatementKind::for_statement:
    case StatementKind::break_statement:
    case StatementKind::continue_statement:
    case StatementKind::return_statement:
    case StatementKind::expression_statement:
    case StatementKind::import_statement:
        return {};
    }

    return {};
}

std::optional<std::string> parameter_hover(const std::vector<Parameter>& parameters, const std::string& name) {
    for (const Parameter& parameter : parameters) {
        if (parameter.name == name) {
            return code_hover("param " + parameter.name + ": " + type_annotation_name(parameter.type));
        }
    }

    return std::nullopt;
}

std::optional<std::string> enum_variant_hover(const Statement& statement, const std::string& name) {
    if (statement.kind != StatementKind::enum_statement) {
        return std::nullopt;
    }

    for (const Parameter& variant : statement.parameters) {
        if (variant.name != name) {
            continue;
        }

        if (variant.type.has_type) {
            return code_hover("variant " + variant.name + "(" + type_name(variant.type.type) + ")");
        }
        return code_hover("variant " + variant.name);
    }

    return std::nullopt;
}

std::optional<std::string> statement_hover(const std::vector<Statement>& statements, const std::string& name) {
    for (const Statement& statement : statements) {
        if (std::optional<std::string> hover = parameter_hover(statement.parameters, name)) {
            return hover;
        }

        if (statement.name == name) {
            std::string hover = declaration_hover(statement);
            if (!hover.empty()) {
                return hover;
            }
        }

        if (std::optional<std::string> hover = enum_variant_hover(statement, name)) {
            return hover;
        }

        if (std::optional<std::string> hover = statement_hover(statement.body, name)) {
            return hover;
        }

        if (std::optional<std::string> hover = statement_hover(statement.else_body, name)) {
            return hover;
        }

        if (statement.initializer != nullptr && statement.initializer->name == name) {
            std::string hover = declaration_hover(*statement.initializer);
            if (!hover.empty()) {
                return hover;
            }
        }

        if (statement.increment != nullptr && statement.increment->name == name) {
            std::string hover = declaration_hover(*statement.increment);
            if (!hover.empty()) {
                return hover;
            }
        }
    }

    return std::nullopt;
}

std::string token_type_after_colon(const std::vector<Token>& tokens, std::size_t colon_index) {
    if (colon_index + 1 >= tokens.size()) {
        return "unknown";
    }

    if (tokens[colon_index].type == TokenType::colon && tokens[colon_index + 1].type != TokenType::eof) {
        return tokens[colon_index + 1].lexeme;
    }

    return "unknown";
}

std::optional<std::string> token_symbol_hover(const std::vector<Token>& tokens, const std::string& name) {
    for (std::size_t index = 0; index + 1 < tokens.size(); ++index) {
        if (tokens[index].type == TokenType::let && tokens[index + 1].lexeme == name) {
            return code_hover("let " + name + ": " + token_type_after_colon(tokens, index + 2));
        }

        if (tokens[index].type == TokenType::const_keyword && tokens[index + 1].lexeme == name) {
            return code_hover("const " + name + ": " + token_type_after_colon(tokens, index + 2));
        }

        if (tokens[index].type == TokenType::fn_keyword && tokens[index + 1].lexeme == name) {
            return code_hover("fn " + name);
        }

        if (tokens[index].type == TokenType::struct_keyword && tokens[index + 1].lexeme == name) {
            return code_hover("struct " + name);
        }

        if (tokens[index].type == TokenType::enum_keyword && tokens[index + 1].lexeme == name) {
            return code_hover("enum " + name);
        }
    }

    return std::nullopt;
}

bool is_visible_module_statement(const Program& program, const Statement& statement) {
    const bool exported_only = module_has_explicit_exports(program);
    return !exported_only || statement.exported;
}

std::optional<std::string> module_member_hover(const Program& program, const std::string& member) {
    for (const Statement& statement : program.statements) {
        const bool visible = is_visible_module_statement(program, statement);
        if (visible && statement.name == member) {
            std::string hover = declaration_hover(statement);
            if (!hover.empty()) {
                return hover;
            }
        }

        if (visible) {
            if (std::optional<std::string> hover = enum_variant_hover(statement, member)) {
                return hover;
            }
        }

        if (statement.kind != StatementKind::impl_statement) {
            continue;
        }

        for (const Statement& method : statement.body) {
            const bool method_visible = !module_has_explicit_exports(program) || statement.exported || method.exported;
            if (method_visible && method.kind == StatementKind::function && method.name == member) {
                return code_hover(function_signature(method));
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> module_hover(const std::string& module_name, const std::filesystem::path& source_directory) {
    if (!find_module_file(module_name, source_directory).has_value()) {
        return std::nullopt;
    }

    return code_hover("module " + module_name);
}

std::optional<std::string> lookup_module_member_hover(const std::string& module_name, const std::string& member,
                                                      const std::filesystem::path& source_directory) {
    const std::optional<std::filesystem::path> path = find_module_file(module_name, source_directory);
    if (!path.has_value()) {
        return std::nullopt;
    }

    const std::optional<Program> program = parse_program_best_effort(read_text_file(*path));
    if (!program.has_value()) {
        return std::nullopt;
    }

    return module_member_hover(*program, member);
}

std::optional<std::string> builtin_hover(const Token& token) {
    switch (token.type) {
    case TokenType::number:
        return code_hover("literal " + token.lexeme + ": int");
    case TokenType::float_number:
        return code_hover("literal " + token.lexeme + ": real");
    case TokenType::char_literal:
        return code_hover("literal " + token.lexeme + ": glyph");
    case TokenType::string_literal:
        return code_hover("literal " + token.lexeme + ": text");
    case TokenType::true_keyword:
    case TokenType::false_keyword:
        return code_hover("literal " + token.lexeme + ": bool");
    case TokenType::identifier:
    case TokenType::plus:
    case TokenType::minus:
    case TokenType::arrow:
    case TokenType::star:
    case TokenType::slash:
    case TokenType::percent:
    case TokenType::bang:
    case TokenType::equal:
    case TokenType::equal_equal:
    case TokenType::fat_arrow:
    case TokenType::bang_equal:
    case TokenType::amp_amp:
    case TokenType::pipe_pipe:
    case TokenType::greater:
    case TokenType::greater_equal:
    case TokenType::less:
    case TokenType::less_equal:
    case TokenType::colon:
    case TokenType::comma:
    case TokenType::dot:
    case TokenType::semicolon:
    case TokenType::left_paren:
    case TokenType::right_paren:
    case TokenType::left_brace:
    case TokenType::right_brace:
    case TokenType::left_bracket:
    case TokenType::right_bracket:
    case TokenType::eof:
        return std::nullopt;
    case TokenType::let:
    case TokenType::const_keyword:
    case TokenType::export_keyword:
    case TokenType::extern_keyword:
    case TokenType::fn_keyword:
    case TokenType::impl_keyword:
    case TokenType::struct_keyword:
    case TokenType::enum_keyword:
    case TokenType::import_keyword:
    case TokenType::match_keyword:
    case TokenType::return_keyword:
    case TokenType::print:
    case TokenType::if_keyword:
    case TokenType::else_keyword:
    case TokenType::while_keyword:
    case TokenType::for_keyword:
    case TokenType::break_keyword:
    case TokenType::continue_keyword:
    case TokenType::as_keyword:
    case TokenType::int_keyword:
    case TokenType::bool_keyword:
    case TokenType::i8_keyword:
    case TokenType::i16_keyword:
    case TokenType::i32_keyword:
    case TokenType::i64_keyword:
    case TokenType::isize_keyword:
    case TokenType::u8_keyword:
    case TokenType::u16_keyword:
    case TokenType::u32_keyword:
    case TokenType::u64_keyword:
    case TokenType::usize_keyword:
    case TokenType::uint8_keyword:
    case TokenType::uint16_keyword:
    case TokenType::uint32_keyword:
    case TokenType::uint64_keyword:
    case TokenType::real32_keyword:
    case TokenType::real64_keyword:
    case TokenType::real_keyword:
    case TokenType::glyph_keyword:
    case TokenType::text_keyword:
    case TokenType::unit_keyword:
        break;
    }

    for (const std::string_view type :
         {"int",   "bool",  "i8",     "i16",    "i32",    "i64",  "isize",  "u8",     "u16",   "u32",  "u64",
          "usize", "uint8", "uint16", "uint32", "uint64", "real", "real32", "real64", "glyph", "text", "unit"}) {
        if (token.lexeme == type) {
            return code_hover("type " + token.lexeme);
        }
    }

    for (const std::string_view bound : {"integer", "numeric", "comparable", "ordered"}) {
        if (token.lexeme == bound) {
            return code_hover("generic bound " + token.lexeme);
        }
    }

    return code_hover("keyword " + token.lexeme);
}

std::string document_text_for(const std::unordered_map<std::string, std::string>& documents, const std::string& uri) {
    const auto document = documents.find(uri);
    if (document != documents.end()) {
        return document->second;
    }

    const std::filesystem::path path = path_from_uri(uri);
    if (!path.empty()) {
        return read_text_file(path);
    }

    return {};
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

std::vector<CompletionItem> complete_source(const std::string& source, const std::string& uri,
                                            const std::filesystem::path& source_directory, std::size_t line,
                                            std::size_t character) {
    std::vector<CompletionItem> completions;
    const std::filesystem::path directory = source_directory_for(uri, source_directory);
    const SourcePosition position{line, character};
    const std::vector<std::string> imports = imports_in_source(source);

    if (std::optional<std::string> qualifier = qualifier_before_cursor(source, position)) {
        if (std::ranges::find(imports, *qualifier) != imports.end()) {
            const std::optional<std::filesystem::path> module_path = find_module_file(*qualifier, directory);
            if (module_path.has_value()) {
                const std::optional<Program> module_program = parse_program_best_effort(read_text_file(*module_path));
                if (module_program.has_value()) {
                    add_module_members(*module_program, completions);
                }
            }
            return completions;
        }

        add_receiver_method_completions(completions);
        return completions;
    }

    add_static_completions(completions);
    add_module_names(directory, completions);
    add_imported_module_completions(imports, completions);
    add_token_symbols(source, completions);

    if (std::optional<Program> program = parse_program_best_effort(source)) {
        add_module_members(*program, completions);
    }

    return completions;
}

std::optional<Hover> hover_source(const std::string& source, const std::string& uri,
                                  const std::filesystem::path& source_directory, std::size_t line,
                                  std::size_t character) {
    const std::filesystem::path directory = source_directory_for(uri, source_directory);
    const std::vector<Token> tokens = tokenize_best_effort(source);
    const std::optional<std::size_t> token_index = token_index_at_position(tokens, SourcePosition{line, character});
    if (!token_index.has_value()) {
        return std::nullopt;
    }

    const Token& token = tokens[*token_index];
    if (*token_index >= 2 && tokens[*token_index - 1].type == TokenType::dot &&
        is_identifier_like(tokens[*token_index - 2])) {
        if (std::optional<std::string> contents =
                lookup_module_member_hover(tokens[*token_index - 2].lexeme, token.lexeme, directory)) {
            return Hover{*contents};
        }
    }

    const std::vector<std::string> imports = imports_in_source(source);
    if (std::ranges::find(imports, token.lexeme) != imports.end()) {
        if (std::optional<std::string> contents = module_hover(token.lexeme, directory)) {
            return Hover{*contents};
        }
    }

    if (std::optional<Program> program = parse_program_best_effort(source)) {
        if (std::optional<std::string> contents = statement_hover(program->statements, token.lexeme)) {
            return Hover{*contents};
        }
    }

    if (std::optional<std::string> contents = token_symbol_hover(tokens, token.lexeme)) {
        return Hover{*contents};
    }

    if (std::optional<std::string> contents = builtin_hover(token)) {
        return Hover{*contents};
    }

    return std::nullopt;
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
                              ",\"result\":{\"capabilities\":{\"textDocumentSync\":1,\"completionProvider\":{"
                              "\"triggerCharacters\":[\".\",\":\"],\"resolveProvider\":false},\"hoverProvider\":true},"
                              "\"serverInfo\":{\"name\":\"dune-"
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

        if (method == "textDocument/completion") {
            const std::string response_id = id.empty() ? "null" : id;
            const std::string uri = find_json_string(message, "uri");
            const std::string text = document_text_for(documents, uri);
            const SourcePosition cursor = find_position(message).value_or(SourcePosition{});
            write_message(
                output, "{\"jsonrpc\":\"2.0\",\"id\":" + response_id + ",\"result\":" +
                            completion_items_json(complete_source(text, uri, {}, cursor.line, cursor.character)) + "}");
            continue;
        }

        if (method == "textDocument/hover") {
            const std::string response_id = id.empty() ? "null" : id;
            const std::string uri = find_json_string(message, "uri");
            const std::string text = document_text_for(documents, uri);
            const SourcePosition cursor = find_position(message).value_or(SourcePosition{});
            const std::optional<Hover> hover = hover_source(text, uri, {}, cursor.line, cursor.character);
            write_message(output, "{\"jsonrpc\":\"2.0\",\"id\":" + response_id +
                                      ",\"result\":" + (hover.has_value() ? hover_json(*hover) : "null") + "}");
            continue;
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
