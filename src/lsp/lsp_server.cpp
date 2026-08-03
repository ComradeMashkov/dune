#include "lsp_server.hpp"

#include "diagnostics/diagnostic.hpp"
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
#include <memory>
#include <optional>
#include <ostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace dune::lsp {

namespace {

#ifndef DUNE_STDLIB_PATH
#define DUNE_STDLIB_PATH "stdlib"
#endif

constexpr std::size_t completion_kind_method = 2;
constexpr std::size_t completion_kind_function = 3;
constexpr std::size_t completion_kind_variable = 6;
constexpr std::size_t completion_kind_interface = 8;
constexpr std::size_t completion_kind_module = 9;
constexpr std::size_t completion_kind_enum = 13;
constexpr std::size_t completion_kind_keyword = 14;
constexpr std::size_t completion_kind_enum_member = 20;
constexpr std::size_t completion_kind_constant = 21;
constexpr std::size_t completion_kind_struct = 22;
constexpr std::size_t completion_kind_type_parameter = 25;

enum class SemanticTokenType : std::size_t {
    namespace_type,
    type,
    struct_type,
    enum_type,
    interface_type,
    type_parameter,
    parameter,
    variable,
    property,
    enum_member,
    function,
    method,
    keyword,
    comment,
    string,
    number,
    operator_type,
    decorator,
};

enum class SemanticTokenModifier : std::size_t {
    declaration,
    definition,
    readonly,
    static_modifier,
    default_library,
    documentation,
    modification,
};

constexpr std::size_t semantic_modifier(SemanticTokenModifier modifier) {
    return std::size_t{1} << static_cast<std::size_t>(modifier);
}

struct SemanticClassification {
    SemanticTokenType type = SemanticTokenType::variable;
    std::size_t modifiers = 0;
};

struct SourceLocationKey {
    std::size_t line = 1;
    std::size_t column = 1;

    bool operator==(const SourceLocationKey&) const = default;
};

struct SourceLocationKeyHash {
    std::size_t operator()(const SourceLocationKey& location) const {
        return location.line * 1315423911U + location.column;
    }
};

using SemanticLocationMap = std::unordered_map<SourceLocationKey, SemanticClassification, SourceLocationKeyHash>;
using SemanticSymbolTable = std::unordered_map<std::string, SemanticClassification>;

struct SourcePosition {
    std::size_t line = 0;
    std::size_t character = 0;
};

struct CheckedProgram {
    Program program;
    std::unordered_map<const Expression*, Type> expression_types;
    std::unordered_map<const Expression*, Type> iterable_element_types;
    std::unordered_map<std::string, TypeChecker::StructDefinition> structs;
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

std::string uri_from_path(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    const std::string native = (error ? path : absolute).generic_string();

    std::string encoded;
    for (const unsigned char character : native) {
        if (std::isalnum(character) != 0 || character == '/' || character == '-' || character == '_' ||
            character == '.' || character == '~' || character == ':') {
            encoded += static_cast<char>(character);
        } else {
            constexpr char digits[] = "0123456789ABCDEF";
            encoded += '%';
            encoded += digits[character >> 4];
            encoded += digits[character & 0x0f];
        }
    }

#if defined(_WIN32)
    return "file:///" + encoded;
#else
    return "file://" + encoded;
#endif
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

// Prefer a front-end error's structured span when it carries one; otherwise fall
// back to parsing the legacy `"line X, columns A-B: "` message shape. The
// structured path avoids the regex entirely for located errors (including parser
// and lexer errors, which previously collapsed to line 1) and is robust to
// messages that themselves contain colons or newlines.
Diagnostic diagnostic_from_exception(const std::exception& error) {
    if (const auto* located = dynamic_cast<const DiagnosticError*>(&error)) {
        const dune::Diagnostic& structured = located->diagnostic();
        if (structured.has_location) {
            return Diagnostic{structured.location.line, structured.location.column,
                              structured.location.column + structured.location.length - 1, structured.message};
        }
    }

    return diagnostic_from_error(error.what());
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
         {"break",   "choice", "const", "continue", "contract", "derive", "else",   "export", "fn",     "foreknown",
          "foreign", "for",    "if",    "import",   "in",       "is",     "method", "print",  "record", "return",
          "static",  "to",     "type",  "when",     "while",    "with",   "true",   "false"}) {
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

// The three Modules v2 import forms as they appear in a source file, scanned
// straight from tokens so the editor can resolve symbols without a full parse.
// `modules` lists every module brought into scope (canonical names, so an alias
// resolves to its real module here); `aliases` maps an `import M as A` alias to
// M; `selective` maps each symbol from `from M import a, b` to its module M.
struct SourceImports {
    std::vector<std::string> modules;
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::string> selective;
};

SourceImports scan_imports(const std::string& source) {
    const std::vector<Token> tokens = tokenize_best_effort(source);
    SourceImports result;
    for (std::size_t index = 0; index + 1 < tokens.size(); ++index) {
        // `import M;` and `import M as A;`
        if (tokens[index].type == TokenType::import_keyword && is_identifier_like(tokens[index + 1])) {
            const std::string module_name = tokens[index + 1].lexeme;
            result.modules.push_back(module_name);
            if (index + 3 < tokens.size() && is_identifier_like(tokens[index + 2]) &&
                tokens[index + 2].lexeme == "as" && is_identifier_like(tokens[index + 3])) {
                result.aliases[tokens[index + 3].lexeme] = module_name;
            }
            continue;
        }

        // `from M import a, b, c;` — `from`/`as` are contextual keywords, so the
        // module and symbols all arrive as plain identifiers.
        if (is_identifier_like(tokens[index]) && tokens[index].lexeme == "from" && index + 2 < tokens.size() &&
            is_identifier_like(tokens[index + 1]) && tokens[index + 2].type == TokenType::import_keyword) {
            const std::string module_name = tokens[index + 1].lexeme;
            result.modules.push_back(module_name);

            bool expect_symbol = true;
            for (std::size_t cursor = index + 3; cursor < tokens.size(); ++cursor) {
                const Token& symbol = tokens[cursor];
                if (symbol.type == TokenType::semicolon || symbol.type == TokenType::eof) {
                    break;
                }
                if (expect_symbol && is_identifier_like(symbol)) {
                    result.selective[symbol.lexeme] = module_name;
                    expect_symbol = false;
                } else if (symbol.type == TokenType::comma) {
                    expect_symbol = true;
                } else {
                    break;
                }
            }
        }
    }

    return result;
}

std::vector<std::string> imports_in_source(const std::string& source) {
    return scan_imports(source).modules;
}

// Resolves an import alias (`import M as A` -> A) to its module, leaving plain
// module names and unknown qualifiers untouched.
std::string resolve_module_alias(const SourceImports& imports, const std::string& qualifier) {
    const auto alias = imports.aliases.find(qualifier);
    return alias == imports.aliases.end() ? qualifier : alias->second;
}

void add_token_symbols(const std::string& source, std::vector<CompletionItem>& completions) {
    const std::vector<Token> tokens = tokenize_best_effort(source);
    for (std::size_t index = 0; index + 1 < tokens.size(); ++index) {
        if (is_identifier_like(tokens[index]) && tokens[index + 1].type == TokenType::left_paren) {
            add_completion(completions, tokens[index].lexeme, "function", completion_kind_function);
        }

        if (is_identifier_like(tokens[index]) && tokens[index + 1].type == TokenType::equal) {
            add_completion(completions, tokens[index].lexeme, "variable", completion_kind_variable);
        }

        if (is_identifier_like(tokens[index]) && tokens[index + 1].type == TokenType::colon) {
            add_completion(completions, tokens[index].lexeme, "variable", completion_kind_variable);
        }

        if (tokens[index].type == TokenType::const_keyword && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "constant", completion_kind_constant);
        }

        if (index + 2 < tokens.size() && tokens[index].type == TokenType::for_keyword &&
            tokens[index + 1].type == TokenType::identifier && tokens[index + 2].type == TokenType::in_keyword) {
            add_completion(completions, tokens[index + 1].lexeme, "loop variable", completion_kind_variable);
        }

        if (tokens[index].type == TokenType::record_keyword && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "record", completion_kind_struct);
        }

        if (tokens[index].type == TokenType::contract_keyword && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "contract", completion_kind_interface);
        }

        if (tokens[index].type == TokenType::choice_keyword && tokens[index + 1].type == TokenType::identifier) {
            add_completion(completions, tokens[index + 1].lexeme, "choice", completion_kind_enum);
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

std::optional<CheckedProgram> check_program_best_effort(const std::string& source,
                                                        const std::filesystem::path& source_directory) {
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        ModuleLoader loader(module_search_paths(source_directory));
        TypeChecker checker;
        Program program = loader.resolve(parser.parse(), source_directory);
        checker.check(program);
        return CheckedProgram{std::move(program), checker.expression_types(), checker.iterable_element_types(),
                              checker.structs()};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

bool module_has_explicit_exports(const Program& program) {
    return std::ranges::any_of(program.statements, [](const Statement& statement) {
        return statement.exported &&
               (statement.kind == StatementKind::function || statement.kind == StatementKind::const_statement ||
                statement.kind == StatementKind::struct_statement || statement.kind == StatementKind::enum_statement ||
                statement.kind == StatementKind::contract_statement ||
                statement.kind == StatementKind::type_alias_statement || statement.kind == StatementKind::method_block);
    });
}

void add_enum_variants(const Statement& statement, bool visible, std::vector<CompletionItem>& completions) {
    if (!visible || statement.kind != StatementKind::enum_statement) {
        return;
    }

    for (const Parameter& variant : statement.parameters) {
        add_completion(completions, variant.name, "choice variant", completion_kind_enum_member);
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
            add_completion(completions, statement.name, "record", completion_kind_struct);
        }

        if (statement.kind == StatementKind::enum_statement && visible) {
            add_completion(completions, statement.name, "choice", completion_kind_enum);
            add_enum_variants(statement, true, completions);
        }

        if (statement.kind == StatementKind::contract_statement && visible) {
            add_completion(completions, statement.name, "contract", completion_kind_interface);
        }

        if (statement.kind == StatementKind::type_alias_statement && visible) {
            add_completion(completions, statement.name, "type alias", completion_kind_type_parameter);
        }

        if (statement.kind == StatementKind::method_block) {
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

void add_common_receiver_method_completions(std::vector<CompletionItem>& completions) {
    for (const std::string_view method :
         {"len",     "push",  "pop",    "clear",  "is_empty", "contains",   "starts_with", "ends_with",
          "char_at", "slice", "prefix", "suffix", "trim",     "trim_start", "trim_end",    "copy",
          "reverse", "first", "last",   "append", "index_of", "count"}) {
        add_completion(completions, std::string(method), "method", completion_kind_method);
    }
}

Type substitute_generic_arguments(const Type& type, const std::vector<GenericParameter>& parameters,
                                  const std::vector<Type>& arguments) {
    if (type.kind == ValueType::generic_type) {
        for (std::size_t index = 0; index < parameters.size() && index < arguments.size(); ++index) {
            if (parameters[index].name == type.name) {
                return arguments[index];
            }
        }
    }

    Type result{type.kind, nullptr};
    result.name = type.name;
    if (type.element != nullptr) {
        result.element = std::make_shared<Type>(substitute_generic_arguments(*type.element, parameters, arguments));
    }

    result.arguments.reserve(type.arguments.size());
    for (const Type& argument : type.arguments) {
        result.arguments.push_back(substitute_generic_arguments(argument, parameters, arguments));
    }

    return result;
}

std::string struct_method_signature(const TypeChecker::StructMethod& method,
                                    const TypeChecker::StructDefinition& record, const Type& receiver) {
    std::string signature = method.name + "(";
    for (std::size_t index = 0; index < method.parameters.size(); ++index) {
        if (index > 0) {
            signature += ", ";
        }

        signature += "arg" + std::to_string(index + 1) + ": " +
                     type_name(substitute_generic_arguments(method.parameters[index], record.generic_parameters,
                                                            receiver.arguments));
    }

    signature += "): " + type_name(substitute_generic_arguments(method.return_type, record.generic_parameters,
                                                                receiver.arguments));
    return signature;
}

bool is_external_record_type(const Type& type) {
    return type.kind == ValueType::struct_type && type.name.find('.') != std::string::npos;
}

void add_struct_method_completions(const Type& receiver,
                                   const std::unordered_map<std::string, TypeChecker::StructDefinition>& structs,
                                   std::vector<CompletionItem>& completions) {
    if (receiver.kind != ValueType::struct_type) {
        return;
    }

    const auto record = structs.find(receiver.name);
    if (record == structs.end()) {
        return;
    }

    for (const TypeChecker::StructMethod& method : record->second.methods) {
        if (method.is_constructor || method.is_static) {
            continue;
        }

        if (is_external_record_type(receiver) && !method.exported) {
            continue;
        }

        add_completion(completions, method.name, struct_method_signature(method, record->second, receiver),
                       completion_kind_method);
    }
}

void add_typed_receiver_method_completions(
    const Type& receiver, const std::unordered_map<std::string, TypeChecker::StructDefinition>& structs,
    std::vector<CompletionItem>& completions) {
    if (receiver.kind == ValueType::array_type) {
        for (const std::string_view method : {"len", "push", "pop", "clear", "is_empty"}) {
            add_completion(completions, std::string(method), "array method", completion_kind_method);
        }
    }

    if (receiver.kind == ValueType::text_type) {
        for (const std::string_view method : {"len", "is_empty", "contains", "starts_with"}) {
            add_completion(completions, std::string(method), "text method", completion_kind_method);
        }
    }

    add_struct_method_completions(receiver, structs, completions);
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

    // Prefer the token the cursor sits inside; only fall back to a token's
    // trailing edge when nothing contains the cursor. Otherwise a click on the
    // start of `x` in `p.x` snaps to the `.` whose end shares that column.
    std::optional<std::size_t> boundary_match;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const Token& token = tokens[index];
        if (token.lexeme.empty() || token.type == TokenType::eof || token.line != target_line) {
            continue;
        }

        const std::size_t start = token.column;
        const std::size_t end = token.column + token.lexeme.size();
        if (target_column >= start && target_column < end) {
            return index;
        }

        if (target_column == end) {
            boundary_match = index;
        }
    }

    return boundary_match;
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
        for (std::size_t bound_index = 0; bound_index < parameters[index].bounds.size(); ++bound_index) {
            text += bound_index == 0 ? " is " : " + ";
            text += parameters[index].bounds[bound_index];
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
    std::string signature = statement.name + generic_parameters_text(statement.generic_parameters) +
                            parameter_list_text(statement.parameters);
    if (statement.type.has_type || statement.kind == StatementKind::function) {
        signature += ": " + type_annotation_name(statement.type, "unit");
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
    case ExpressionKind::tuple:
        return "(unknown, unknown)";
    case ExpressionKind::struct_literal:
        return expression.lexeme;
    case ExpressionKind::array_comprehension:
    case ExpressionKind::identifier:
    case ExpressionKind::index:
    case ExpressionKind::slice:
    case ExpressionKind::member:
    case ExpressionKind::unary:
    case ExpressionKind::try_expression:
    case ExpressionKind::cast:
    case ExpressionKind::binary:
    case ExpressionKind::range:
    case ExpressionKind::when_expression:
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

std::optional<Type> statement_value_type(const Statement& statement,
                                         const std::unordered_map<const Expression*, Type>& expression_types) {
    if (statement.type.has_type) {
        return statement.type.type;
    }

    if (statement.expression == nullptr) {
        return std::nullopt;
    }

    const auto type = expression_types.find(statement.expression.get());
    if (type == expression_types.end()) {
        return std::nullopt;
    }

    return type->second;
}

std::optional<Type> symbol_type_in_statement(const Statement& statement, const std::string& name,
                                             const std::unordered_map<const Expression*, Type>& expression_types) {
    for (const Parameter& parameter : statement.parameters) {
        if (parameter.name == name && parameter.type.has_type) {
            return parameter.type.type;
        }
    }

    if ((statement.kind == StatementKind::binding || statement.kind == StatementKind::const_statement) &&
        statement.name == name) {
        if (std::optional<Type> type = statement_value_type(statement, expression_types)) {
            return type;
        }
    }

    if (statement.kind == StatementKind::assign && statement.target != nullptr &&
        statement.target->kind == ExpressionKind::identifier && statement.target->lexeme == name) {
        if (std::optional<Type> type = statement_value_type(statement, expression_types)) {
            return type;
        }
    }

    if (statement.initializer != nullptr) {
        if (std::optional<Type> type = symbol_type_in_statement(*statement.initializer, name, expression_types)) {
            return type;
        }
    }

    if (statement.increment != nullptr) {
        if (std::optional<Type> type = symbol_type_in_statement(*statement.increment, name, expression_types)) {
            return type;
        }
    }

    for (auto child = statement.body.rbegin(); child != statement.body.rend(); ++child) {
        if (std::optional<Type> type = symbol_type_in_statement(*child, name, expression_types)) {
            return type;
        }
    }

    for (auto child = statement.else_body.rbegin(); child != statement.else_body.rend(); ++child) {
        if (std::optional<Type> type = symbol_type_in_statement(*child, name, expression_types)) {
            return type;
        }
    }

    return std::nullopt;
}

std::optional<Type> symbol_type_in_program(const CheckedProgram& checked, const std::string& name) {
    for (auto statement = checked.program.statements.rbegin(); statement != checked.program.statements.rend();
         ++statement) {
        if (statement->kind == StatementKind::import_statement) {
            break;
        }

        if (std::optional<Type> type = symbol_type_in_statement(*statement, name, checked.expression_types)) {
            return type;
        }
    }

    return std::nullopt;
}

std::optional<CheckedProgram> check_program_prefix_before(const std::string& source,
                                                          const std::filesystem::path& source_directory,
                                                          SourcePosition position) {
    const std::size_t offset = offset_at_position(source, position);
    const std::size_t line_start = source.rfind('\n', offset == 0 ? 0 : offset - 1);
    const std::size_t prefix_end = line_start == std::string::npos ? 0 : line_start + 1;
    if (prefix_end == 0) {
        return std::nullopt;
    }

    return check_program_best_effort(source.substr(0, prefix_end), source_directory);
}

std::string code_hover(std::string declaration) {
    return "```dune\n" + std::move(declaration) + "\n```";
}

std::string trim_ascii(const std::string& text) {
    const std::size_t begin = text.find_first_not_of(" \t\r");
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(" \t\r");
    return text.substr(begin, end - begin + 1);
}

// Matches a leading `tag:` / `tag ` prefix on a doc line and returns the rest.
bool match_doc_tag(const std::string& line, std::string_view tag, std::string& rest) {
    if (line.size() < tag.size() || line.compare(0, tag.size(), tag) != 0) {
        return false;
    }
    std::size_t index = tag.size();
    if (index < line.size() && line[index] == ':') {
        ++index;
    } else if (index != line.size() && line[index] != ' ') {
        return false;
    }
    rest = trim_ascii(line.substr(index));
    return true;
}

// Renders a stored doc-comment into hover markdown. Recognises the structured
// tags brief/param/returns/example (Doxygen/JSDoc style); anything untagged is
// treated as plain description, so ordinary `//` comments render as prose.
std::string render_doc_comment(const std::string& doc) {
    std::vector<std::string> description;
    std::vector<std::pair<std::string, std::string>> params;
    std::string returns;
    std::vector<std::string> example;
    bool saw_tag = false;

    std::size_t line_start = 0;
    while (line_start <= doc.size()) {
        const std::size_t newline = doc.find('\n', line_start);
        const std::size_t end = newline == std::string::npos ? doc.size() : newline;
        const std::string line = trim_ascii(doc.substr(line_start, end - line_start));
        line_start = newline == std::string::npos ? doc.size() + 1 : newline + 1;

        std::string rest;
        if (match_doc_tag(line, "brief", rest)) {
            saw_tag = true;
            if (!rest.empty()) {
                description.push_back(rest);
            }
        } else if (match_doc_tag(line, "param", rest)) {
            saw_tag = true;
            const std::size_t colon = rest.find(':');
            std::string name = colon == std::string::npos ? rest : trim_ascii(rest.substr(0, colon));
            std::string detail = colon == std::string::npos ? std::string{} : trim_ascii(rest.substr(colon + 1));
            if (name.empty()) {
                continue;
            }
            const std::size_t space = name.find(' ');
            if (colon == std::string::npos && space != std::string::npos) {
                detail = trim_ascii(name.substr(space + 1));
                name = name.substr(0, space);
            }
            params.emplace_back(std::move(name), std::move(detail));
        } else if (match_doc_tag(line, "returns", rest) || match_doc_tag(line, "return", rest)) {
            saw_tag = true;
            returns = rest;
        } else if (match_doc_tag(line, "example", rest)) {
            saw_tag = true;
            if (!rest.empty()) {
                example.push_back(rest);
            }
        } else if (saw_tag && !example.empty()) {
            // Continuation lines after an `example:` tag extend the code block.
            example.push_back(line);
        } else {
            description.push_back(line);
        }
    }

    std::string markdown;
    auto append_block = [&markdown](const std::string& block) {
        if (block.empty()) {
            return;
        }
        if (!markdown.empty()) {
            markdown += "\n\n";
        }
        markdown += block;
    };

    // Untagged/description lines: blank lines separate paragraphs, otherwise
    // adjacent lines flow together.
    std::string paragraph;
    std::string description_block;
    auto flush_paragraph = [&]() {
        if (paragraph.empty()) {
            return;
        }
        if (!description_block.empty()) {
            description_block += "\n\n";
        }
        description_block += paragraph;
        paragraph.clear();
    };
    for (const std::string& text : description) {
        if (text.empty()) {
            flush_paragraph();
            continue;
        }
        if (!paragraph.empty()) {
            paragraph += ' ';
        }
        paragraph += text;
    }
    flush_paragraph();
    append_block(description_block);

    if (!params.empty()) {
        std::string block = "**Parameters:**";
        for (const auto& [name, detail] : params) {
            block += "\n- `" + name + "`";
            if (!detail.empty()) {
                block += " — " + detail;
            }
        }
        append_block(block);
    }

    if (!returns.empty()) {
        append_block("**Returns:** " + returns);
    }

    if (!example.empty()) {
        std::string block = "**Example:**\n```dune";
        for (const std::string& text : example) {
            block += "\n" + text;
        }
        block += "\n```";
        append_block(block);
    }

    return markdown;
}

// Appends a rendered doc-comment beneath a signature code block.
std::string with_doc(std::string code, const std::string& doc) {
    if (doc.empty()) {
        return code;
    }
    const std::string rendered = render_doc_comment(doc);
    if (rendered.empty()) {
        return code;
    }
    return code + "\n\n---\n\n" + rendered;
}

std::string declaration_hover(const Statement& statement) {
    std::string signature;
    switch (statement.kind) {
    case StatementKind::binding:
        signature = statement.name + ": " + variable_type_text(statement);
        break;
    case StatementKind::const_statement:
        signature = (statement.is_foreknown ? "foreknown const " : "const ") + statement.name + ": " +
                    variable_type_text(statement);
        break;
    case StatementKind::function:
        signature = function_signature(statement);
        break;
    case StatementKind::struct_statement:
        signature = "record " + statement.name + generic_parameters_text(statement.generic_parameters);
        break;
    case StatementKind::enum_statement:
        signature = "choice " + statement.name + generic_parameters_text(statement.generic_parameters);
        break;
    case StatementKind::contract_statement:
        signature = "contract " + statement.name;
        break;
    case StatementKind::type_alias_statement:
        signature = "type " + statement.name + generic_parameters_text(statement.generic_parameters) + " = " +
                    type_annotation_name(statement.type);
        break;
    case StatementKind::method_block:
    case StatementKind::assign:
    case StatementKind::block:
    case StatementKind::if_statement:
    case StatementKind::while_statement:
    case StatementKind::for_statement:
    case StatementKind::for_in_statement:
    case StatementKind::break_statement:
    case StatementKind::continue_statement:
    case StatementKind::return_statement:
    case StatementKind::expression_statement:
    case StatementKind::import_statement:
    case StatementKind::module_declaration:
    case StatementKind::test_block:
        return {};
    }

    if (signature.empty()) {
        return {};
    }

    return with_doc(code_hover(std::move(signature)), statement.doc_comment);
}

std::optional<std::string> parameter_hover(const std::vector<Parameter>& parameters, const std::string& name) {
    for (const Parameter& parameter : parameters) {
        if (parameter.name == name) {
            return with_doc(code_hover("param " + parameter.name + ": " + type_annotation_name(parameter.type)),
                            parameter.doc_comment);
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
            return code_hover("choice variant " + variant.name + "(" + type_name(variant.type.type) + ")");
        }
        return code_hover("choice variant " + variant.name);
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

std::string typed_variable_hover(const Statement& statement,
                                 const std::unordered_map<const Expression*, Type>& expression_types,
                                 const std::unordered_map<const Expression*, Type>& iterable_element_types) {
    if (statement.kind == StatementKind::for_in_statement) {
        if (statement.expression == nullptr) {
            return {};
        }

        const auto type = iterable_element_types.find(statement.expression.get());
        if (type == iterable_element_types.end()) {
            return {};
        }

        return with_doc(code_hover(statement.name + ": " + type_name(type->second)), statement.doc_comment);
    }

    if (statement.kind != StatementKind::binding && statement.kind != StatementKind::const_statement &&
        statement.kind != StatementKind::assign) {
        return {};
    }
    if (statement.expression == nullptr) {
        return {};
    }

    const auto type = expression_types.find(statement.expression.get());
    if (type == expression_types.end()) {
        return {};
    }

    if (statement.kind == StatementKind::const_statement) {
        return with_doc(code_hover((statement.is_foreknown ? "foreknown const " : "const ") + statement.name + ": " +
                                   type_name(type->second)),
                        statement.doc_comment);
    }

    const std::string name = statement.target != nullptr && statement.target->kind == ExpressionKind::identifier
                                 ? statement.target->lexeme
                                 : statement.name;
    return with_doc(code_hover(name + ": " + type_name(type->second)), statement.doc_comment);
}

std::optional<std::string> typed_statement_hover(const std::vector<Statement>& statements, const std::string& name,
                                                 const std::unordered_map<const Expression*, Type>& expression_types,
                                                 const std::unordered_map<const Expression*, Type>&
                                                     iterable_element_types) {
    for (auto statement = statements.rbegin(); statement != statements.rend(); ++statement) {
        if (std::optional<std::string> hover = parameter_hover(statement->parameters, name)) {
            return hover;
        }

        const bool is_named_assign = statement->kind == StatementKind::assign && statement->target != nullptr &&
                                     statement->target->kind == ExpressionKind::identifier &&
                                     statement->target->lexeme == name;
        if (statement->name == name || is_named_assign) {
            std::string hover = typed_variable_hover(*statement, expression_types, iterable_element_types);
            if (!hover.empty()) {
                return hover;
            }

            hover = declaration_hover(*statement);
            if (!hover.empty()) {
                return hover;
            }
        }

        if (std::optional<std::string> hover = enum_variant_hover(*statement, name)) {
            return hover;
        }

        if (std::optional<std::string> hover =
                typed_statement_hover(statement->body, name, expression_types, iterable_element_types)) {
            return hover;
        }

        if (std::optional<std::string> hover =
                typed_statement_hover(statement->else_body, name, expression_types, iterable_element_types)) {
            return hover;
        }

        if (statement->initializer != nullptr && statement->initializer->name == name) {
            std::string hover = typed_variable_hover(*statement->initializer, expression_types, iterable_element_types);
            if (!hover.empty()) {
                return hover;
            }

            hover = declaration_hover(*statement->initializer);
            if (!hover.empty()) {
                return hover;
            }
        }

        if (statement->increment != nullptr && statement->increment->name == name) {
            std::string hover = typed_variable_hover(*statement->increment, expression_types, iterable_element_types);
            if (!hover.empty()) {
                return hover;
            }

            hover = declaration_hover(*statement->increment);
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
        if (is_identifier_like(tokens[index]) && tokens[index].lexeme == name &&
            tokens[index + 1].type == TokenType::equal) {
            return code_hover(name + ": unknown");
        }

        if (is_identifier_like(tokens[index]) && tokens[index].lexeme == name &&
            tokens[index + 1].type == TokenType::colon) {
            return code_hover(name + ": " + token_type_after_colon(tokens, index + 1));
        }

        if (tokens[index].type == TokenType::const_keyword && tokens[index + 1].lexeme == name) {
            return code_hover("const " + name + ": " + token_type_after_colon(tokens, index + 2));
        }

        if (is_identifier_like(tokens[index]) && tokens[index].lexeme == name &&
            tokens[index + 1].type == TokenType::left_paren) {
            return code_hover(name + "(...)");
        }

        if (tokens[index].type == TokenType::record_keyword && tokens[index + 1].lexeme == name) {
            return code_hover("record " + name);
        }

        if (tokens[index].type == TokenType::contract_keyword && tokens[index + 1].lexeme == name) {
            return code_hover("contract " + name);
        }

        if (tokens[index].type == TokenType::choice_keyword && tokens[index + 1].lexeme == name) {
            return code_hover("choice " + name);
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

        if (statement.kind != StatementKind::method_block) {
            continue;
        }

        for (const Statement& method : statement.body) {
            const bool method_visible = !module_has_explicit_exports(program) || statement.exported || method.exported;
            if (method_visible && method.kind == StatementKind::function && method.name == member) {
                return with_doc(code_hover(function_signature(method)), method.doc_comment);
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string> builtin_receiver_method_hover(const Type& receiver, const std::string& method) {
    if (receiver.kind == ValueType::array_type) {
        if (method == "len") {
            return code_hover("len(): int");
        }
        if (method == "push") {
            const std::string element = receiver.element == nullptr ? "unknown" : type_name(*receiver.element);
            return code_hover("push(value: " + element + "): unit");
        }
        if (method == "pop") {
            const std::string element = receiver.element == nullptr ? "unknown" : type_name(*receiver.element);
            return code_hover("pop(): " + element);
        }
        if (method == "clear") {
            return code_hover("clear(): unit");
        }
        if (method == "is_empty") {
            return code_hover("is_empty(): bool");
        }
    }

    if (receiver.kind == ValueType::text_type) {
        if (method == "len") {
            return code_hover("len(): int");
        }
        if (method == "is_empty") {
            return code_hover("is_empty(): bool");
        }
        if (method == "contains") {
            return code_hover("contains(needle: text): bool");
        }
        if (method == "starts_with") {
            return code_hover("starts_with(prefix: text): bool");
        }
    }

    return std::nullopt;
}

std::optional<std::string>
receiver_method_hover(const Type& receiver, const std::string& method,
                      const std::unordered_map<std::string, TypeChecker::StructDefinition>& structs) {
    if (std::optional<std::string> hover = builtin_receiver_method_hover(receiver, method)) {
        return hover;
    }

    if (receiver.kind != ValueType::struct_type) {
        return std::nullopt;
    }

    const auto record = structs.find(receiver.name);
    if (record == structs.end()) {
        return std::nullopt;
    }

    std::string signatures;
    std::string doc;
    for (const TypeChecker::StructMethod& candidate : record->second.methods) {
        if (candidate.is_constructor || candidate.is_static || candidate.name != method) {
            continue;
        }

        if (is_external_record_type(receiver) && !candidate.exported) {
            continue;
        }

        if (!signatures.empty()) {
            signatures += "\n";
        }
        signatures += struct_method_signature(candidate, record->second, receiver);
        if (doc.empty()) {
            doc = candidate.doc_comment;
        }
    }

    if (signatures.empty()) {
        return std::nullopt;
    }

    return with_doc(code_hover(signatures), doc);
}

// Hover for a record field accessed as `value.field`, respecting cross-module
// visibility. Fields declared with a doc-comment show it beneath the type.
std::optional<std::string>
receiver_field_hover(const Type& receiver, const std::string& field,
                     const std::unordered_map<std::string, TypeChecker::StructDefinition>& structs) {
    if (receiver.kind != ValueType::struct_type) {
        return std::nullopt;
    }

    const auto record = structs.find(receiver.name);
    if (record == structs.end()) {
        return std::nullopt;
    }

    for (const TypeChecker::StructField& candidate : record->second.fields) {
        if (candidate.name != field) {
            continue;
        }

        if (is_external_record_type(receiver) && !candidate.exported) {
            return std::nullopt;
        }

        return with_doc(code_hover(candidate.name + ": " + type_name(candidate.type)), candidate.doc_comment);
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
    case TokenType::dot_dot:
    case TokenType::question:
    case TokenType::semicolon:
    case TokenType::left_paren:
    case TokenType::right_paren:
    case TokenType::left_brace:
    case TokenType::right_brace:
    case TokenType::left_bracket:
    case TokenType::right_bracket:
    case TokenType::eof:
        return std::nullopt;
    case TokenType::const_keyword:
    case TokenType::export_keyword:
    case TokenType::foreknown_keyword:
    case TokenType::foreign_keyword:
    case TokenType::fn_keyword:
    case TokenType::method_keyword:
    case TokenType::record_keyword:
    case TokenType::contract_keyword:
    case TokenType::with_keyword:
    case TokenType::derive_keyword:
    case TokenType::choice_keyword:
    case TokenType::import_keyword:
    case TokenType::when_keyword:
    case TokenType::return_keyword:
    case TokenType::if_keyword:
    case TokenType::else_keyword:
    case TokenType::while_keyword:
    case TokenType::for_keyword:
    case TokenType::in_keyword:
    case TokenType::break_keyword:
    case TokenType::continue_keyword:
    case TokenType::test_keyword:
    case TokenType::static_keyword:
    case TokenType::to_keyword:
    case TokenType::type_keyword:
    case TokenType::is_keyword:
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

bool is_builtin_type_token(TokenType type) {
    switch (type) {
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
        return true;
    default:
        return false;
    }
}

bool is_semantic_keyword_token(TokenType type) {
    return type >= TokenType::const_keyword && type <= TokenType::false_keyword && !is_builtin_type_token(type);
}

bool is_semantic_operator_token(TokenType type) {
    switch (type) {
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
    case TokenType::dot_dot:
    case TokenType::question:
        return true;
    default:
        return false;
    }
}

std::vector<Token> tokenize_semantic_best_effort(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens;
    while (true) {
        try {
            Token token = lexer.next_token();
            tokens.push_back(token);
            if (token.type == TokenType::eof) {
                break;
            }
        } catch (const std::exception&) {
            break;
        }
    }
    return tokens;
}

std::size_t utf16_length(std::string_view value) {
    std::size_t length = 0;
    for (std::size_t index = 0; index < value.size();) {
        const unsigned char lead = static_cast<unsigned char>(value[index]);
        std::size_t width = 1;
        std::size_t units = 1;
        if ((lead & 0xe0U) == 0xc0U && index + 1 < value.size()) {
            width = 2;
        } else if ((lead & 0xf0U) == 0xe0U && index + 2 < value.size()) {
            width = 3;
        } else if ((lead & 0xf8U) == 0xf0U && index + 3 < value.size()) {
            width = 4;
            units = 2;
        }
        index += width;
        length += units;
    }
    return length;
}

std::vector<std::size_t> source_line_starts(const std::string& source) {
    std::vector<std::size_t> starts{0};
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (source[index] == '\n') {
            starts.push_back(index + 1);
        }
    }
    return starts;
}

void add_semantic_span(std::vector<SemanticToken>& result, const std::string& source,
                       const std::vector<std::size_t>& line_starts, std::size_t line, std::size_t byte_start,
                       std::size_t byte_length, SemanticClassification classification) {
    if (line >= line_starts.size() || byte_length == 0) {
        return;
    }

    const std::size_t line_start = line_starts[line];
    const std::size_t line_end = line + 1 < line_starts.size() ? line_starts[line + 1] - 1 : source.size();
    if (line_start + byte_start >= line_end || line_start + byte_start + byte_length > line_end) {
        return;
    }

    const std::string_view prefix(source.data() + line_start, byte_start);
    const std::string_view token(source.data() + line_start + byte_start, byte_length);
    result.push_back(SemanticToken{line, utf16_length(prefix), utf16_length(token),
                                   static_cast<std::size_t>(classification.type), classification.modifiers});
}

void add_comment_span(std::vector<SemanticToken>& result, const std::string& source,
                      const std::vector<std::size_t>& line_starts, std::size_t begin, std::size_t end,
                      bool documentation) {
    const SemanticClassification classification{
        SemanticTokenType::comment,
        documentation ? semantic_modifier(SemanticTokenModifier::documentation) : 0,
    };
    std::size_t cursor = begin;
    while (cursor < end) {
        const auto upper = std::upper_bound(line_starts.begin(), line_starts.end(), cursor);
        const std::size_t line = static_cast<std::size_t>(std::distance(line_starts.begin(), upper) - 1);
        const std::size_t segment_end = std::min(end, source.find('\n', cursor));
        const std::size_t actual_end = segment_end == std::string::npos ? end : segment_end;
        const std::size_t token_end =
            actual_end > cursor && source[actual_end - 1] == '\r' ? actual_end - 1 : actual_end;
        if (token_end > cursor) {
            add_semantic_span(result, source, line_starts, line, cursor - line_starts[line], token_end - cursor,
                              classification);
        }
        if (actual_end >= end) {
            break;
        }
        cursor = actual_end + 1;
    }
}

void add_comment_tokens(std::vector<SemanticToken>& result, const std::string& source,
                        const std::vector<std::size_t>& line_starts) {
    for (std::size_t index = 0; index < source.size();) {
        if (source[index] == '"' || source[index] == '\'') {
            const char quote = source[index++];
            while (index < source.size()) {
                if (source[index] == '\\' && index + 1 < source.size()) {
                    index += 2;
                } else if (source[index++] == quote) {
                    break;
                }
            }
            continue;
        }

        if (source[index] != '/' || index + 1 >= source.size()) {
            ++index;
            continue;
        }

        if (source[index + 1] == '/') {
            const std::size_t begin = index;
            const bool documentation = index + 2 < source.size() && source[index + 2] == '/';
            index = source.find('\n', index + 2);
            if (index == std::string::npos) {
                index = source.size();
            }
            add_comment_span(result, source, line_starts, begin, index, documentation);
            continue;
        }

        if (source[index + 1] == '*') {
            const std::size_t begin = index;
            const bool documentation = index + 2 < source.size() && source[index + 2] == '*';
            const std::size_t close = source.find("*/", index + 2);
            index = close == std::string::npos ? source.size() : close + 2;
            add_comment_span(result, source, line_starts, begin, index, documentation);
            continue;
        }

        ++index;
    }
}

SemanticClassification declared_symbol(SemanticTokenType type, std::size_t modifiers = 0, bool is_definition = true) {
    modifiers |= semantic_modifier(SemanticTokenModifier::declaration);
    if (is_definition) {
        modifiers |= semantic_modifier(SemanticTokenModifier::definition);
    }
    return SemanticClassification{type, modifiers};
}

void mark_semantic_location(SemanticLocationMap& locations, SourceLocation location,
                            SemanticClassification classification) {
    if (location.line != 0 && location.column != 0) {
        locations[SourceLocationKey{location.line, location.column}] = classification;
    }
}

SemanticClassification declaration_classification(const Statement& statement) {
    switch (statement.kind) {
    case StatementKind::function:
        return declared_symbol(SemanticTokenType::function);
    case StatementKind::const_statement:
        return declared_symbol(SemanticTokenType::variable, semantic_modifier(SemanticTokenModifier::readonly));
    case StatementKind::struct_statement:
        return declared_symbol(SemanticTokenType::struct_type);
    case StatementKind::enum_statement:
        return declared_symbol(SemanticTokenType::enum_type);
    case StatementKind::contract_statement:
        return declared_symbol(SemanticTokenType::interface_type);
    case StatementKind::type_alias_statement:
        return declared_symbol(SemanticTokenType::type);
    default:
        return declared_symbol(SemanticTokenType::variable);
    }
}

bool is_semantic_declaration_kind(StatementKind kind) {
    switch (kind) {
    case StatementKind::binding:
    case StatementKind::const_statement:
    case StatementKind::function:
    case StatementKind::struct_statement:
    case StatementKind::enum_statement:
    case StatementKind::contract_statement:
    case StatementKind::type_alias_statement:
        return true;
    default:
        return false;
    }
}

void collect_global_semantic_symbols(const Program& program, SemanticSymbolTable& symbols) {
    for (const Statement& statement : program.statements) {
        if (is_semantic_declaration_kind(statement.kind)) {
            symbols[statement.name] = declaration_classification(statement);
        }
        if (statement.kind == StatementKind::enum_statement) {
            for (const Parameter& variant : statement.parameters) {
                symbols[variant.name] = declared_symbol(SemanticTokenType::enum_member);
            }
        }
        if (statement.kind == StatementKind::method_block) {
            for (const Statement& method : statement.body) {
                symbols[method.name] = declared_symbol(
                    SemanticTokenType::method,
                    method.is_static_record_member ? semantic_modifier(SemanticTokenModifier::static_modifier) : 0);
            }
        }
    }
}

void collect_semantic_metadata(const std::vector<Statement>& statements,
                               std::unordered_set<std::string>& type_parameters,
                               std::unordered_set<std::string>& static_methods) {
    for (const Statement& statement : statements) {
        for (const GenericParameter& parameter : statement.generic_parameters) {
            type_parameters.insert(parameter.name);
        }
        if (statement.kind == StatementKind::function && statement.is_static_record_member) {
            static_methods.insert(statement.name);
        }
        collect_semantic_metadata(statement.body, type_parameters, static_methods);
        collect_semantic_metadata(statement.else_body, type_parameters, static_methods);
    }
}

using SemanticScopes = std::vector<SemanticSymbolTable>;

std::optional<SemanticClassification>
lookup_semantic_symbol(const SemanticScopes& scopes, const SemanticSymbolTable& globals, const std::string& name) {
    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
        if (const auto found = scope->find(name); found != scope->end()) {
            SemanticClassification reference = found->second;
            reference.modifiers &= ~(semantic_modifier(SemanticTokenModifier::declaration) |
                                     semantic_modifier(SemanticTokenModifier::definition));
            return reference;
        }
    }
    if (const auto found = globals.find(name); found != globals.end()) {
        SemanticClassification reference = found->second;
        reference.modifiers &= ~(semantic_modifier(SemanticTokenModifier::declaration) |
                                 semantic_modifier(SemanticTokenModifier::definition));
        return reference;
    }
    return std::nullopt;
}

bool is_builtin_function_name(std::string_view name) {
    return name == "print" || name == "format" || name.starts_with("__");
}

void classify_expression_semantics(const Expression& expression, SemanticLocationMap& locations,
                                   const SemanticScopes& scopes, const SemanticSymbolTable& globals,
                                   bool modification = false) {
    if (expression.kind == ExpressionKind::identifier) {
        SemanticClassification classification = lookup_semantic_symbol(scopes, globals, expression.lexeme)
                                                    .value_or(SemanticClassification{SemanticTokenType::variable, 0});
        if (expression.lexeme == "this" || expression.lexeme == "self") {
            classification = SemanticClassification{SemanticTokenType::parameter,
                                                    semantic_modifier(SemanticTokenModifier::readonly)};
        }
        if (modification) {
            classification.modifiers |= semantic_modifier(SemanticTokenModifier::modification);
        }
        mark_semantic_location(locations, expression.location, classification);
        return;
    }

    if (expression.kind == ExpressionKind::call) {
        SemanticClassification classification = lookup_semantic_symbol(scopes, globals, expression.lexeme)
                                                    .value_or(SemanticClassification{SemanticTokenType::function, 0});
        if (is_builtin_function_name(expression.lexeme)) {
            classification = SemanticClassification{SemanticTokenType::function,
                                                    semantic_modifier(SemanticTokenModifier::default_library)};
        }
        mark_semantic_location(locations, expression.location, classification);
        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            classify_expression_semantics(*argument, locations, scopes, globals);
        }
        return;
    }

    if (expression.kind == ExpressionKind::struct_literal) {
        SemanticClassification classification{SemanticTokenType::struct_type, 0};
        if (const auto found = globals.find(expression.lexeme); found != globals.end()) {
            classification = found->second;
            classification.modifiers &= ~(semantic_modifier(SemanticTokenModifier::declaration) |
                                          semantic_modifier(SemanticTokenModifier::definition));
        }
        // Qualified literals keep the location of their module qualifier; the
        // token pass classifies `module.Record` without overwriting `module`.
        if (expression.lexeme.find('.') == std::string::npos) {
            mark_semantic_location(locations, expression.location, classification);
        }
        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            classify_expression_semantics(*argument, locations, scopes, globals);
        }
        return;
    }

    if (expression.kind == ExpressionKind::array_comprehension) {
        if (expression.right != nullptr) {
            classify_expression_semantics(*expression.right, locations, scopes, globals);
        }
        SemanticScopes comprehension_scopes = scopes;
        comprehension_scopes.push_back({{expression.lexeme, SemanticClassification{SemanticTokenType::variable, 0}}});
        if (expression.left != nullptr) {
            classify_expression_semantics(*expression.left, locations, comprehension_scopes, globals);
        }
        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            classify_expression_semantics(*argument, locations, comprehension_scopes, globals);
        }
        return;
    }

    if (expression.left != nullptr) {
        classify_expression_semantics(*expression.left, locations, scopes, globals, modification);
    }
    if (expression.right != nullptr) {
        classify_expression_semantics(*expression.right, locations, scopes, globals);
    }
    for (const std::unique_ptr<Expression>& argument : expression.arguments) {
        classify_expression_semantics(*argument, locations, scopes, globals);
    }
}

void classify_statement_semantics(const std::vector<Statement>& statements, SemanticLocationMap& locations,
                                  SemanticScopes& scopes, const SemanticSymbolTable& globals,
                                  std::optional<SemanticClassification> callable_classification = std::nullopt);

void classify_callable_semantics(const Statement& statement, SemanticClassification classification,
                                 SemanticLocationMap& locations, SemanticScopes& scopes,
                                 const SemanticSymbolTable& globals) {
    mark_semantic_location(locations, statement.location, classification);
    SemanticSymbolTable callable_scope;
    for (const GenericParameter& parameter : statement.generic_parameters) {
        const SemanticClassification generic = declared_symbol(SemanticTokenType::type_parameter);
        mark_semantic_location(locations, parameter.location, generic);
        callable_scope[parameter.name] = generic;
    }
    for (const Parameter& parameter : statement.parameters) {
        const SemanticClassification value = declared_symbol(SemanticTokenType::parameter, 0, false);
        mark_semantic_location(locations, parameter.location, value);
        callable_scope[parameter.name] = value;
    }
    scopes.push_back(std::move(callable_scope));
    classify_statement_semantics(statement.body, locations, scopes, globals);
    scopes.pop_back();
}

void classify_statement_semantics(const std::vector<Statement>& statements, SemanticLocationMap& locations,
                                  SemanticScopes& scopes, const SemanticSymbolTable& globals,
                                  std::optional<SemanticClassification> callable_classification) {
    for (const Statement& statement : statements) {
        switch (statement.kind) {
        case StatementKind::function:
            classify_callable_semantics(statement,
                                        callable_classification.value_or(declaration_classification(statement)),
                                        locations, scopes, globals);
            break;
        case StatementKind::method_block:
            for (const Statement& method : statement.body) {
                classify_callable_semantics(
                    method,
                    declared_symbol(
                        SemanticTokenType::method,
                        method.is_static_record_member ? semantic_modifier(SemanticTokenModifier::static_modifier) : 0),
                    locations, scopes, globals);
            }
            break;
        case StatementKind::struct_statement:
            for (const GenericParameter& generic : statement.generic_parameters) {
                mark_semantic_location(locations, generic.location, declared_symbol(SemanticTokenType::type_parameter));
            }
            for (const Parameter& field : statement.parameters) {
                mark_semantic_location(locations, field.location, declared_symbol(SemanticTokenType::property));
                if (field.default_value != nullptr) {
                    classify_expression_semantics(*field.default_value, locations, scopes, globals);
                }
            }
            for (const Statement& method : statement.body) {
                classify_callable_semantics(
                    method,
                    declared_symbol(
                        SemanticTokenType::method,
                        method.is_static_record_member ? semantic_modifier(SemanticTokenModifier::static_modifier) : 0),
                    locations, scopes, globals);
            }
            break;
        case StatementKind::enum_statement:
            for (const GenericParameter& generic : statement.generic_parameters) {
                mark_semantic_location(locations, generic.location, declared_symbol(SemanticTokenType::type_parameter));
            }
            for (const Parameter& variant : statement.parameters) {
                mark_semantic_location(locations, variant.location, declared_symbol(SemanticTokenType::enum_member));
            }
            break;
        case StatementKind::contract_statement:
            for (const Statement& method : statement.body) {
                classify_callable_semantics(method, declared_symbol(SemanticTokenType::method, 0, false), locations,
                                            scopes, globals);
            }
            break;
        case StatementKind::type_alias_statement:
            for (const GenericParameter& generic : statement.generic_parameters) {
                mark_semantic_location(locations, generic.location, declared_symbol(SemanticTokenType::type_parameter));
            }
            break;
        case StatementKind::const_statement:
            if (statement.expression != nullptr) {
                classify_expression_semantics(*statement.expression, locations, scopes, globals);
            }
            if (!scopes.empty()) {
                scopes.back()[statement.name] = SemanticClassification{
                    SemanticTokenType::variable, semantic_modifier(SemanticTokenModifier::readonly)};
            }
            break;
        case StatementKind::binding:
            if (statement.expression != nullptr) {
                classify_expression_semantics(*statement.expression, locations, scopes, globals);
            }
            mark_semantic_location(locations, statement.location,
                                   declared_symbol(SemanticTokenType::variable, 0, false));
            if (!scopes.empty()) {
                scopes.back()[statement.name] = SemanticClassification{SemanticTokenType::variable, 0};
            }
            break;
        case StatementKind::assign:
            if (statement.target != nullptr) {
                classify_expression_semantics(*statement.target, locations, scopes, globals, true);
            }
            if (statement.expression != nullptr) {
                classify_expression_semantics(*statement.expression, locations, scopes, globals);
            }
            break;
        case StatementKind::for_in_statement: {
            if (statement.expression != nullptr) {
                classify_expression_semantics(*statement.expression, locations, scopes, globals);
            }
            scopes.push_back({{statement.name, SemanticClassification{SemanticTokenType::variable, 0}}});
            classify_statement_semantics(statement.body, locations, scopes, globals);
            scopes.pop_back();
            break;
        }
        case StatementKind::for_statement: {
            scopes.emplace_back();
            if (statement.initializer != nullptr) {
                const Statement& initializer = *statement.initializer;
                if (initializer.expression != nullptr) {
                    classify_expression_semantics(*initializer.expression, locations, scopes, globals);
                }
                if (initializer.target != nullptr) {
                    classify_expression_semantics(*initializer.target, locations, scopes, globals, true);
                }
                if (initializer.kind == StatementKind::binding) {
                    mark_semantic_location(locations, initializer.location,
                                           declared_symbol(SemanticTokenType::variable, 0, false));
                    scopes.back()[initializer.name] = SemanticClassification{SemanticTokenType::variable, 0};
                }
            }
            if (statement.expression != nullptr) {
                classify_expression_semantics(*statement.expression, locations, scopes, globals);
            }
            classify_statement_semantics(statement.body, locations, scopes, globals);
            if (statement.increment != nullptr) {
                if (statement.increment->target != nullptr) {
                    classify_expression_semantics(*statement.increment->target, locations, scopes, globals, true);
                }
                if (statement.increment->expression != nullptr) {
                    classify_expression_semantics(*statement.increment->expression, locations, scopes, globals);
                }
            }
            scopes.pop_back();
            break;
        }
        default:
            if (statement.expression != nullptr) {
                classify_expression_semantics(*statement.expression, locations, scopes, globals);
            }
            if (statement.target != nullptr) {
                classify_expression_semantics(*statement.target, locations, scopes, globals, true);
            }
            if (statement.initializer != nullptr) {
                if (statement.initializer->expression != nullptr) {
                    classify_expression_semantics(*statement.initializer->expression, locations, scopes, globals);
                }
            }
            if (statement.increment != nullptr && statement.increment->expression != nullptr) {
                classify_expression_semantics(*statement.increment->expression, locations, scopes, globals);
            }
            if (!statement.body.empty()) {
                scopes.emplace_back();
                classify_statement_semantics(statement.body, locations, scopes, globals);
                scopes.pop_back();
            }
            if (!statement.else_body.empty()) {
                scopes.emplace_back();
                classify_statement_semantics(statement.else_body, locations, scopes, globals);
                scopes.pop_back();
            }
            break;
        }
    }
}

bool path_is_within(const std::filesystem::path& path, const std::filesystem::path& root) {
    std::error_code error;
    const std::filesystem::path canonical_path = std::filesystem::weakly_canonical(path, error);
    if (error) {
        return false;
    }
    const std::filesystem::path canonical_root = std::filesystem::weakly_canonical(root, error);
    if (error) {
        return false;
    }
    const auto mismatch =
        std::mismatch(canonical_root.begin(), canonical_root.end(), canonical_path.begin(), canonical_path.end());
    return mismatch.first == canonical_root.end();
}

bool is_standard_library_module(const std::string& module_name, const std::filesystem::path& source_directory) {
    const std::optional<std::filesystem::path> module = find_module_file(module_name, source_directory);
    if (!module.has_value()) {
        return false;
    }
    if (path_is_within(*module, DUNE_STDLIB_PATH)) {
        return true;
    }
    const char* configured = std::getenv("DUNE_STDLIB_PATH");
    if (configured == nullptr) {
        return false;
    }
#if defined(_WIN32)
    constexpr char delimiter = ';';
#else
    constexpr char delimiter = ':';
#endif
    std::stringstream roots(configured);
    std::string root;
    while (std::getline(roots, root, delimiter)) {
        if (!root.empty() && path_is_within(*module, root)) {
            return true;
        }
    }
    return false;
}

struct SemanticModuleInfo {
    SemanticSymbolTable members;
    std::size_t modifiers = 0;
};

using SemanticModuleTable = std::unordered_map<std::string, SemanticModuleInfo>;

SemanticModuleInfo load_semantic_module(const std::string& module_name, const std::filesystem::path& source_directory) {
    SemanticModuleInfo result;
    if (is_standard_library_module(module_name, source_directory)) {
        result.modifiers = semantic_modifier(SemanticTokenModifier::default_library);
    }
    const std::optional<std::filesystem::path> path = find_module_file(module_name, source_directory);
    if (!path.has_value()) {
        return result;
    }
    const std::optional<Program> program = parse_program_best_effort(read_text_file(*path));
    if (!program.has_value()) {
        return result;
    }
    for (const Statement& statement : program->statements) {
        if (is_semantic_declaration_kind(statement.kind)) {
            SemanticClassification classification = declaration_classification(statement);
            classification.modifiers = result.modifiers;
            result.members[statement.name] = classification;
        }
        if (statement.kind == StatementKind::enum_statement) {
            for (const Parameter& variant : statement.parameters) {
                result.members[variant.name] = SemanticClassification{SemanticTokenType::enum_member, result.modifiers};
            }
        }
        if (statement.kind == StatementKind::method_block) {
            for (const Statement& method : statement.body) {
                result.members[method.name] = SemanticClassification{SemanticTokenType::method, result.modifiers};
            }
        }
    }
    return result;
}

std::optional<SemanticClassification> module_member_semantic_classification(const SemanticModuleTable& modules,
                                                                            const std::string& module_name,
                                                                            const std::string& member) {
    const auto module = modules.find(module_name);
    if (module == modules.end()) {
        return std::nullopt;
    }
    const auto symbol = module->second.members.find(member);
    if (symbol != module->second.members.end()) {
        return symbol->second;
    }
    return std::nullopt;
}

bool token_is_identifier(const Token& token) {
    return token.type == TokenType::identifier || token.type == TokenType::text_keyword;
}

void set_token_classification(std::vector<std::optional<SemanticClassification>>& classifications, std::size_t index,
                              SemanticClassification classification) {
    if (index < classifications.size()) {
        classifications[index] = classification;
    }
}

void classify_import_tokens(const std::vector<Token>& tokens,
                            std::vector<std::optional<SemanticClassification>>& classifications,
                            const SourceImports& imports, const SemanticModuleTable& modules) {
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (tokens[index].type == TokenType::import_keyword && index + 1 < tokens.size() &&
            token_is_identifier(tokens[index + 1])) {
            const auto module = modules.find(tokens[index + 1].lexeme);
            const std::size_t modifiers = module == modules.end() ? 0 : module->second.modifiers;
            set_token_classification(classifications, index + 1,
                                     SemanticClassification{SemanticTokenType::namespace_type, modifiers});
            if (index + 3 < tokens.size() && token_is_identifier(tokens[index + 2]) &&
                tokens[index + 2].lexeme == "as" && token_is_identifier(tokens[index + 3])) {
                set_token_classification(classifications, index + 2,
                                         SemanticClassification{SemanticTokenType::keyword, 0});
                set_token_classification(classifications, index + 3,
                                         declared_symbol(SemanticTokenType::namespace_type, modifiers, false));
            }
        }

        if (token_is_identifier(tokens[index]) && tokens[index].lexeme == "from" && index + 2 < tokens.size() &&
            token_is_identifier(tokens[index + 1]) && tokens[index + 2].type == TokenType::import_keyword) {
            set_token_classification(classifications, index, SemanticClassification{SemanticTokenType::keyword, 0});
            const std::string& module = tokens[index + 1].lexeme;
            const auto module_info = modules.find(module);
            const std::size_t modifiers = module_info == modules.end() ? 0 : module_info->second.modifiers;
            set_token_classification(classifications, index + 1,
                                     SemanticClassification{SemanticTokenType::namespace_type, modifiers});
            for (std::size_t cursor = index + 3;
                 cursor < tokens.size() && tokens[cursor].type != TokenType::semicolon &&
                 tokens[cursor].type != TokenType::eof;
                 ++cursor) {
                if (!token_is_identifier(tokens[cursor])) {
                    continue;
                }
                set_token_classification(classifications, cursor,
                                         module_member_semantic_classification(modules, module, tokens[cursor].lexeme)
                                             .value_or(SemanticClassification{SemanticTokenType::variable, modifiers}));
            }
        }
    }

    for (std::size_t index = 2; index < tokens.size(); ++index) {
        if (tokens[index - 1].type != TokenType::dot || !token_is_identifier(tokens[index]) ||
            !token_is_identifier(tokens[index - 2])) {
            continue;
        }
        const std::string module = resolve_module_alias(imports, tokens[index - 2].lexeme);
        if (std::ranges::find(imports.modules, module) == imports.modules.end()) {
            continue;
        }
        set_token_classification(classifications, index,
                                 module_member_semantic_classification(modules, module, tokens[index].lexeme)
                                     .value_or(SemanticClassification{
                                         index + 1 < tokens.size() && tokens[index + 1].type == TokenType::left_paren
                                             ? SemanticTokenType::function
                                             : SemanticTokenType::property,
                                         modules.contains(module) ? modules.at(module).modifiers : 0}));
    }
}

std::vector<SemanticToken> build_semantic_tokens(const std::string& source,
                                                 const std::filesystem::path& source_directory) {
    const std::vector<Token> tokens = tokenize_semantic_best_effort(source);
    std::vector<std::optional<SemanticClassification>> classifications(tokens.size());
    const std::vector<std::size_t> line_starts = source_line_starts(source);

    SemanticLocationMap locations;
    SemanticSymbolTable globals;
    std::unordered_set<std::string> type_parameters;
    std::unordered_set<std::string> static_methods;
    const SourceImports imports = scan_imports(source);
    SemanticModuleTable modules;
    for (const std::string& module : imports.modules) {
        modules.try_emplace(module, load_semantic_module(module, source_directory));
        globals[module] =
            SemanticClassification{SemanticTokenType::namespace_type, modules.find(module)->second.modifiers};
    }
    for (const auto& [alias, module] : imports.aliases) {
        globals[alias] =
            SemanticClassification{SemanticTokenType::namespace_type, modules.find(module)->second.modifiers};
    }
    for (const auto& [symbol, module] : imports.selective) {
        globals[symbol] = module_member_semantic_classification(modules, module, symbol)
                              .value_or(SemanticClassification{SemanticTokenType::variable, 0});
    }
    if (std::optional<Program> program = parse_program_best_effort(source)) {
        collect_global_semantic_symbols(*program, globals);
        collect_semantic_metadata(program->statements, type_parameters, static_methods);
        SemanticScopes scopes(1);
        classify_statement_semantics(program->statements, locations, scopes, globals);
    }

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const Token& token = tokens[index];
        if (is_builtin_type_token(token.type)) {
            classifications[index] = SemanticClassification{SemanticTokenType::type,
                                                            semantic_modifier(SemanticTokenModifier::default_library)};
        } else if (is_semantic_keyword_token(token.type)) {
            classifications[index] = SemanticClassification{SemanticTokenType::keyword, 0};
        } else if (token.type == TokenType::number || token.type == TokenType::float_number) {
            classifications[index] = SemanticClassification{SemanticTokenType::number, 0};
        } else if (token.type == TokenType::char_literal || token.type == TokenType::string_literal) {
            classifications[index] = SemanticClassification{SemanticTokenType::string, 0};
        } else if (is_semantic_operator_token(token.type)) {
            classifications[index] = SemanticClassification{SemanticTokenType::operator_type, 0};
        }
    }

    classify_import_tokens(tokens, classifications, imports, modules);

    for (std::size_t index = 0; index + 1 < tokens.size(); ++index) {
        const Token& token = tokens[index];
        if (token.type == TokenType::record_keyword && token_is_identifier(tokens[index + 1])) {
            set_token_classification(classifications, index + 1, declared_symbol(SemanticTokenType::struct_type));
        } else if (token.type == TokenType::choice_keyword && token_is_identifier(tokens[index + 1])) {
            set_token_classification(classifications, index + 1, declared_symbol(SemanticTokenType::enum_type));
        } else if (token.type == TokenType::contract_keyword && token_is_identifier(tokens[index + 1])) {
            set_token_classification(classifications, index + 1, declared_symbol(SemanticTokenType::interface_type));
        } else if (token.type == TokenType::type_keyword && token_is_identifier(tokens[index + 1])) {
            set_token_classification(classifications, index + 1, declared_symbol(SemanticTokenType::type));
        } else if (token.type == TokenType::const_keyword && token_is_identifier(tokens[index + 1])) {
            set_token_classification(classifications, index + 1,
                                     declared_symbol(SemanticTokenType::variable,
                                                     semantic_modifier(SemanticTokenModifier::readonly), false));
        } else if (token.type == TokenType::fn_keyword && token_is_identifier(tokens[index + 1])) {
            set_token_classification(classifications, index + 1, declared_symbol(SemanticTokenType::function));
        } else if (token.type == TokenType::for_keyword && index + 2 < tokens.size() &&
                   token_is_identifier(tokens[index + 1]) && tokens[index + 2].type == TokenType::in_keyword) {
            set_token_classification(classifications, index + 1,
                                     declared_symbol(SemanticTokenType::variable, 0, false));
        }
        if (token_is_identifier(token) && token.lexeme == "module" && token_is_identifier(tokens[index + 1])) {
            set_token_classification(classifications, index, SemanticClassification{SemanticTokenType::keyword, 0});
            set_token_classification(classifications, index + 1, declared_symbol(SemanticTokenType::namespace_type));
        }
        if (token.type == TokenType::derive_keyword) {
            for (std::size_t cursor = index + 1; cursor < tokens.size() && tokens[cursor].type != TokenType::left_brace;
                 ++cursor) {
                if (token_is_identifier(tokens[cursor])) {
                    set_token_classification(classifications, cursor,
                                             SemanticClassification{SemanticTokenType::decorator, 0});
                }
            }
        }
    }

    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (!token_is_identifier(tokens[index])) {
            continue;
        }
        if (const auto exact = locations.find(SourceLocationKey{tokens[index].line, tokens[index].column});
            exact != locations.end()) {
            classifications[index] = exact->second;
            continue;
        }
        if (classifications[index].has_value()) {
            continue;
        }
        if (index > 0 && tokens[index - 1].type == TokenType::dot) {
            const bool builtin_method = tokens[index].lexeme == "len" || tokens[index].lexeme == "push" ||
                                        tokens[index].lexeme == "pop" || tokens[index].lexeme == "clear" ||
                                        tokens[index].lexeme == "is_empty" || tokens[index].lexeme == "contains" ||
                                        tokens[index].lexeme == "starts_with";
            classifications[index] = SemanticClassification{
                index + 1 < tokens.size() && tokens[index + 1].type == TokenType::left_paren
                    ? SemanticTokenType::method
                    : SemanticTokenType::property,
                (static_methods.contains(tokens[index].lexeme)
                     ? semantic_modifier(SemanticTokenModifier::static_modifier)
                     : 0) |
                    (builtin_method ? semantic_modifier(SemanticTokenModifier::default_library) : 0)};
            continue;
        }
        if (type_parameters.contains(tokens[index].lexeme)) {
            classifications[index] = SemanticClassification{SemanticTokenType::type_parameter, 0};
            continue;
        }
        const bool imported_namespace =
            imports.aliases.contains(tokens[index].lexeme) ||
            std::ranges::find(imports.modules, tokens[index].lexeme) != imports.modules.end();
        if (imported_namespace) {
            const std::string module = resolve_module_alias(imports, tokens[index].lexeme);
            classifications[index] = SemanticClassification{
                SemanticTokenType::namespace_type, modules.contains(module) ? modules.at(module).modifiers : 0};
            continue;
        }
        if (const auto selective = imports.selective.find(tokens[index].lexeme); selective != imports.selective.end()) {
            classifications[index] =
                module_member_semantic_classification(modules, selective->second, tokens[index].lexeme)
                    .value_or(SemanticClassification{SemanticTokenType::variable, 0});
            continue;
        }
        if (const auto global = globals.find(tokens[index].lexeme); global != globals.end()) {
            SemanticClassification reference = global->second;
            reference.modifiers &= ~(semantic_modifier(SemanticTokenModifier::declaration) |
                                     semantic_modifier(SemanticTokenModifier::definition));
            classifications[index] = reference;
            continue;
        }
        if (index + 1 < tokens.size() && tokens[index + 1].type == TokenType::left_paren) {
            classifications[index] = SemanticClassification{
                SemanticTokenType::function, is_builtin_function_name(tokens[index].lexeme)
                                                 ? semantic_modifier(SemanticTokenModifier::default_library)
                                                 : 0};
            continue;
        }
        if (index + 1 < tokens.size() && tokens[index + 1].type == TokenType::colon) {
            classifications[index] = SemanticClassification{SemanticTokenType::property, 0};
            continue;
        }
        if ((index > 0 &&
             (tokens[index - 1].type == TokenType::colon || tokens[index - 1].type == TokenType::with_keyword ||
              tokens[index - 1].type == TokenType::is_keyword || tokens[index - 1].type == TokenType::to_keyword)) ||
            (!tokens[index].lexeme.empty() && std::isupper(static_cast<unsigned char>(tokens[index].lexeme.front())))) {
            classifications[index] = SemanticClassification{SemanticTokenType::type, 0};
            continue;
        }
        classifications[index] = SemanticClassification{SemanticTokenType::variable, 0};
    }

    std::vector<SemanticToken> result;
    result.reserve(tokens.size());
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (!classifications[index].has_value() || tokens[index].type == TokenType::eof) {
            continue;
        }
        add_semantic_span(result, source, line_starts, tokens[index].line - 1, tokens[index].column - 1,
                          tokens[index].lexeme.size(), classifications[index].value_or(SemanticClassification{}));
    }
    add_comment_tokens(result, source, line_starts);
    std::ranges::sort(result, [](const SemanticToken& left, const SemanticToken& right) {
        return std::tie(left.line, left.start_character, left.length) <
               std::tie(right.line, right.start_character, right.length);
    });
    result.erase(std::unique(result.begin(), result.end(),
                             [](const SemanticToken& left, const SemanticToken& right) {
                                 return left.line == right.line && left.start_character == right.start_character &&
                                        left.length == right.length;
                             }),
                 result.end());
    return result;
}

std::string semantic_token_names_json(const std::vector<std::string>& names) {
    std::string json = "[";
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index != 0) {
            json += ',';
        }
        json += '"' + json_escape(names[index]) + '"';
    }
    return json + ']';
}

std::string semantic_tokens_json(const std::vector<SemanticToken>& tokens) {
    std::string json = "{\"data\":[";
    std::size_t previous_line = 0;
    std::size_t previous_start = 0;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const SemanticToken& token = tokens[index];
        const std::size_t delta_line = token.line - previous_line;
        const std::size_t delta_start =
            delta_line == 0 ? token.start_character - previous_start : token.start_character;
        if (index != 0) {
            json += ',';
        }
        json += std::to_string(delta_line) + ',' + std::to_string(delta_start) + ',' + std::to_string(token.length) +
                ',' + std::to_string(token.token_type) + ',' + std::to_string(token.token_modifiers);
        previous_line = token.line;
        previous_start = token.start_character;
    }
    return json + "]}";
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

const std::vector<std::string>& semantic_token_types() {
    static const std::vector<std::string> names = {
        "namespace", "type",     "struct",   "enum",       "interface", "typeParameter",
        "parameter", "variable", "property", "enumMember", "function",  "method",
        "keyword",   "comment",  "string",   "number",     "operator",  "decorator",
    };
    return names;
}

const std::vector<std::string>& semantic_token_modifiers() {
    static const std::vector<std::string> names = {
        "declaration", "definition", "readonly", "static", "defaultLibrary", "documentation", "modification",
    };
    return names;
}

std::vector<SemanticToken> semantic_tokens_source(const std::string& source, const std::string& uri,
                                                  const std::filesystem::path& source_directory) {
    return build_semantic_tokens(source, source_directory_for(uri, source_directory));
}

std::vector<Diagnostic> diagnose_source(const std::string& source, const std::string& uri,
                                        const std::filesystem::path& source_directory) {
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        const std::filesystem::path directory = source_directory_for(uri, source_directory);
        ModuleLoader loader(module_search_paths(directory));
        TypeChecker checker;
        checker.check(loader.resolve(parser.parse(), directory));
        return {};
    } catch (const std::exception& error) {
        return {diagnostic_from_exception(error)};
    }
}

std::vector<CompletionItem> complete_source(const std::string& source, const std::string& uri,
                                            const std::filesystem::path& source_directory, std::size_t line,
                                            std::size_t character) {
    std::vector<CompletionItem> completions;
    const std::filesystem::path directory = source_directory_for(uri, source_directory);
    const SourcePosition position{line, character};
    const SourceImports scanned = scan_imports(source);
    const std::vector<std::string>& imports = scanned.modules;

    if (std::optional<std::string> qualifier = qualifier_before_cursor(source, position)) {
        const std::string module_qualifier = resolve_module_alias(scanned, *qualifier);
        if (std::ranges::find(imports, module_qualifier) != imports.end()) {
            const std::optional<std::filesystem::path> module_path = find_module_file(module_qualifier, directory);
            if (module_path.has_value()) {
                const std::optional<Program> module_program = parse_program_best_effort(read_text_file(*module_path));
                if (module_program.has_value()) {
                    add_module_members(*module_program, completions);
                }
            }
            return completions;
        }

        std::optional<CheckedProgram> checked = check_program_best_effort(source, directory);
        if (!checked.has_value()) {
            checked = check_program_prefix_before(source, directory, position);
        }

        if (checked.has_value()) {
            if (std::optional<Type> receiver = symbol_type_in_program(*checked, *qualifier)) {
                add_typed_receiver_method_completions(*receiver, checked->structs, completions);
            }
        }

        if (completions.empty()) {
            add_common_receiver_method_completions(completions);
        }
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

    const SourceImports imports = scan_imports(source);
    const Token& token = tokens[*token_index];
    if (*token_index >= 2 && tokens[*token_index - 1].type == TokenType::dot &&
        is_identifier_like(tokens[*token_index - 2])) {
        // `module.member` or `alias.member`: resolve the alias to the real module.
        const std::string qualifier = resolve_module_alias(imports, tokens[*token_index - 2].lexeme);
        if (std::ranges::find(imports.modules, qualifier) != imports.modules.end()) {
            if (std::optional<std::string> contents = lookup_module_member_hover(qualifier, token.lexeme, directory)) {
                return Hover{*contents};
            }
        } else if (std::optional<CheckedProgram> checked = check_program_best_effort(source, directory)) {
            if (std::optional<Type> receiver = symbol_type_in_program(*checked, qualifier)) {
                if (std::optional<std::string> contents =
                        receiver_method_hover(*receiver, token.lexeme, checked->structs)) {
                    return Hover{*contents};
                }
                if (std::optional<std::string> contents =
                        receiver_field_hover(*receiver, token.lexeme, checked->structs)) {
                    return Hover{*contents};
                }
            }
        }
    }

    // A bare module name, whether written directly or through an `as` alias.
    if (std::ranges::find(imports.modules, token.lexeme) != imports.modules.end() ||
        imports.aliases.contains(token.lexeme)) {
        if (std::optional<std::string> contents =
                module_hover(resolve_module_alias(imports, token.lexeme), directory)) {
            return Hover{*contents};
        }
    }

    if (std::optional<CheckedProgram> checked = check_program_best_effort(source, directory)) {
        if (std::optional<std::string> contents =
                typed_statement_hover(checked->program.statements, token.lexeme, checked->expression_types,
                                      checked->iterable_element_types)) {
            return Hover{*contents};
        }
    }

    if (std::optional<Program> program = parse_program_best_effort(source)) {
        if (std::optional<std::string> contents = statement_hover(program->statements, token.lexeme)) {
            return Hover{*contents};
        }
    }

    // A symbol pulled in unqualified by `from M import ...`, once local lookups
    // have had their chance (a same-file declaration shadows the import).
    if (const auto selective = imports.selective.find(token.lexeme); selective != imports.selective.end()) {
        if (std::optional<std::string> contents =
                lookup_module_member_hover(selective->second, token.lexeme, directory)) {
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

bool declares_named_symbol(StatementKind kind) {
    switch (kind) {
    case StatementKind::binding:
    case StatementKind::const_statement:
    case StatementKind::function:
    case StatementKind::struct_statement:
    case StatementKind::enum_statement:
    case StatementKind::contract_statement:
    case StatementKind::type_alias_statement:
        return true;
    default:
        return false;
    }
}

// Walks the AST for the declaration that introduces `name` and returns where it
// is written. Mirrors statement_hover's traversal so go-to-definition and hover
// agree on which symbol a token refers to. Parameters cover function arguments,
// record fields, and choice variants; the first match in document order wins.
std::optional<SourceLocation> find_declaration_location(const std::vector<Statement>& statements,
                                                        const std::string& name) {
    for (const Statement& statement : statements) {
        for (const Parameter& parameter : statement.parameters) {
            if (parameter.name == name) {
                return parameter.location;
            }
        }

        if (statement.name == name &&
            (declares_named_symbol(statement.kind) || statement.kind == StatementKind::for_in_statement)) {
            return statement.location;
        }

        if (std::optional<SourceLocation> found = find_declaration_location(statement.body, name)) {
            return found;
        }

        if (std::optional<SourceLocation> found = find_declaration_location(statement.else_body, name)) {
            return found;
        }

        if (statement.initializer != nullptr && statement.initializer->name == name &&
            declares_named_symbol(statement.initializer->kind)) {
            return statement.initializer->location;
        }

        if (statement.increment != nullptr && statement.increment->name == name &&
            declares_named_symbol(statement.increment->kind)) {
            return statement.increment->location;
        }
    }

    return std::nullopt;
}

// Locates an exported `member` inside a resolved module program. Mirrors
// module_member_hover's visibility rules so go-to-definition lands on the same
// declaration hover describes, including methods declared in `method` blocks.
std::optional<SourceLocation> find_module_member_location(const Program& program, const std::string& member) {
    for (const Statement& statement : program.statements) {
        const bool visible = is_visible_module_statement(program, statement);
        if (visible && statement.name == member && declares_named_symbol(statement.kind)) {
            return statement.location;
        }

        if (visible && statement.kind == StatementKind::enum_statement) {
            for (const Parameter& variant : statement.parameters) {
                if (variant.name == member) {
                    return variant.location;
                }
            }
        }

        if (statement.kind != StatementKind::method_block) {
            continue;
        }

        for (const Statement& method : statement.body) {
            const bool method_visible = !module_has_explicit_exports(program) || statement.exported || method.exported;
            if (method_visible && method.kind == StatementKind::function && method.name == member) {
                return method.location;
            }
        }
    }

    return std::nullopt;
}

// Resolves `module.member` to the member's declaration in the module file.
std::optional<DefinitionLocation> lookup_module_member_definition(const std::string& module_name,
                                                                  const std::string& member,
                                                                  const std::filesystem::path& source_directory) {
    const std::optional<std::filesystem::path> path = find_module_file(module_name, source_directory);
    if (!path.has_value()) {
        return std::nullopt;
    }

    const std::optional<Program> program = parse_program_best_effort(read_text_file(*path));
    if (!program.has_value()) {
        return std::nullopt;
    }

    const std::optional<SourceLocation> location = find_module_member_location(*program, member);
    if (!location.has_value()) {
        return std::nullopt;
    }

    return DefinitionLocation{uri_from_path(*path), location->line, location->column, location->length};
}

// Finds the identifier token under the cursor. Unlike token_index_at_position,
// this only considers identifiers and resolves boundary clicks toward the
// identifier: on `p.x`, a click at the start of `x` must not snap to the `.` to
// its left (whose inclusive end shares that column).
std::optional<std::size_t> identifier_token_at_position(const std::vector<Token>& tokens, SourcePosition position) {
    const std::size_t target_line = position.line + 1;
    const std::size_t target_column = position.character + 1;

    std::optional<std::size_t> boundary_match;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        const Token& token = tokens[index];
        if (!is_identifier_like(token) || token.line != target_line) {
            continue;
        }

        const std::size_t start = token.column;
        const std::size_t end = token.column + token.lexeme.size();
        if (target_column >= start && target_column < end) {
            return index;
        }

        if (target_column == end) {
            boundary_match = index;
        }
    }

    return boundary_match;
}

std::optional<DefinitionLocation> definition_source(const std::string& source, const std::string& uri,
                                                    const std::filesystem::path& source_directory, std::size_t line,
                                                    std::size_t character) {
    const std::filesystem::path directory = source_directory_for(uri, source_directory);
    const std::vector<Token> tokens = tokenize_best_effort(source);
    const SourcePosition position{line, character};
    // Do not let a keyword click drift to a nearby identifier at a token
    // boundary. In particular, `foreknown` and `const` must behave identically
    // and can never be definition targets.
    if (const std::optional<std::size_t> token = token_index_at_position(tokens, position); token.has_value()) {
        const TokenType type = tokens[token.value_or(0)].type;
        if (type == TokenType::foreknown_keyword || type == TokenType::const_keyword) {
            return std::nullopt;
        }
    }
    const std::optional<std::size_t> token_index = identifier_token_at_position(tokens, position);
    if (!token_index.has_value()) {
        return std::nullopt;
    }

    const Token& token = tokens[*token_index];
    const SourceImports imports = scan_imports(source);

    // A `qualifier.member` access: the qualifier is either an imported module
    // (possibly via an `as` alias) or a value whose type carries the method.
    if (*token_index >= 2 && tokens[*token_index - 1].type == TokenType::dot &&
        is_identifier_like(tokens[*token_index - 2])) {
        const std::string qualifier = resolve_module_alias(imports, tokens[*token_index - 2].lexeme);

        if (std::ranges::find(imports.modules, qualifier) != imports.modules.end()) {
            // `module.member` -> the member's declaration inside the module file.
            if (std::optional<DefinitionLocation> member =
                    lookup_module_member_definition(qualifier, token.lexeme, directory)) {
                return member;
            }
        } else if (std::optional<CheckedProgram> checked = check_program_best_effort(source, directory)) {
            // `value.method()` -> the method on the receiver's record type. A
            // module-qualified type name (`matrix.Vector`) sends us to that
            // module file; a local record keeps us in this document.
            if (std::optional<Type> receiver = symbol_type_in_program(*checked, qualifier);
                receiver.has_value() && receiver->kind == ValueType::struct_type) {
                const auto record = checked->structs.find(receiver->name);
                if (record != checked->structs.end()) {
                    for (const TypeChecker::StructMethod& method : record->second.methods) {
                        if (method.is_static || method.is_constructor || method.name != token.lexeme) {
                            continue;
                        }

                        const std::size_t dot = receiver->name.find('.');
                        if (dot == std::string::npos) {
                            return DefinitionLocation{uri, method.location.line, method.location.column,
                                                      method.location.length};
                        }

                        if (const std::optional<std::filesystem::path> path =
                                find_module_file(receiver->name.substr(0, dot), directory)) {
                            return DefinitionLocation{uri_from_path(*path), method.location.line,
                                                      method.location.column, method.location.length};
                        }
                    }
                }
            }
        }
    }

    // A bare module name opens the module file at its top. This covers `math` in
    // `import math`, the module in `from array import ...`, and an `as` alias.
    if (std::ranges::find(imports.modules, token.lexeme) != imports.modules.end() ||
        imports.aliases.contains(token.lexeme)) {
        if (const std::optional<std::filesystem::path> path =
                find_module_file(resolve_module_alias(imports, token.lexeme), directory)) {
            return DefinitionLocation{uri_from_path(*path), 1, 1, 1};
        }
    }

    // A same-file declaration.
    const std::optional<Program> program = parse_program_best_effort(source);
    if (!program.has_value()) {
        return std::nullopt;
    }

    if (const std::optional<SourceLocation> location = find_declaration_location(program->statements, token.lexeme)) {
        return DefinitionLocation{uri, location->line, location->column, location->length};
    }

    // A symbol brought in unqualified by `from M import ...` (checked after the
    // same-file pass so a local declaration of the same name wins).
    if (const auto selective = imports.selective.find(token.lexeme); selective != imports.selective.end()) {
        if (std::optional<DefinitionLocation> member =
                lookup_module_member_definition(selective->second, token.lexeme, directory)) {
            return member;
        }
    }

    return std::nullopt;
}

std::string definition_json(const DefinitionLocation& definition) {
    const std::size_t line = definition.line == 0 ? 0 : definition.line - 1;
    const std::size_t start = definition.column == 0 ? 0 : definition.column - 1;
    const std::size_t end = start + definition.length;
    return "{\"uri\":\"" + json_escape(definition.uri) + "\",\"range\":{\"start\":{\"line\":" + std::to_string(line) +
           ",\"character\":" + std::to_string(start) + "},\"end\":{\"line\":" + std::to_string(line) +
           ",\"character\":" + std::to_string(end) + "}}}";
}

int run(std::istream& input, std::ostream& output) {
    std::unordered_map<std::string, std::string> documents;

    std::string message;
    while (read_message(input, message)) {
        const std::string method = find_json_string(message, "method");
        const std::string id = find_raw_id(message);

        if (method == "initialize") {
            const std::string response_id = id.empty() ? "null" : id;
            const std::string semantic_legend =
                "{\"tokenTypes\":" + semantic_token_names_json(semantic_token_types()) +
                ",\"tokenModifiers\":" + semantic_token_names_json(semantic_token_modifiers()) + "}";
            std::string response = "{\"jsonrpc\":\"2.0\",\"id\":";
            response += response_id;
            response += ",\"result\":{\"capabilities\":{\"textDocumentSync\":1,\"completionProvider\":{"
                        "\"triggerCharacters\":[\".\",\":\"],\"resolveProvider\":false},\"hoverProvider\":true,"
                        "\"definitionProvider\":true,\"semanticTokensProvider\":{\"legend\":";
            response += semantic_legend;
            response +=
                ",\"full\":true,\"range\":false}},\"serverInfo\":{\"name\":\"dune-lsp\",\"version\":\"0.1.0\"}}}";
            write_message(output, response);
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

        if (method == "textDocument/definition") {
            const std::string response_id = id.empty() ? "null" : id;
            const std::string uri = find_json_string(message, "uri");
            const std::string text = document_text_for(documents, uri);
            const SourcePosition cursor = find_position(message).value_or(SourcePosition{});
            const std::optional<DefinitionLocation> definition =
                definition_source(text, uri, {}, cursor.line, cursor.character);
            write_message(output, "{\"jsonrpc\":\"2.0\",\"id\":" + response_id + ",\"result\":" +
                                      (definition.has_value() ? definition_json(*definition) : "null") + "}");
            continue;
        }

        if (method == "textDocument/semanticTokens/full") {
            const std::string response_id = id.empty() ? "null" : id;
            const std::string uri = find_json_string(message, "uri");
            const std::string text = document_text_for(documents, uri);
            write_message(output, "{\"jsonrpc\":\"2.0\",\"id\":" + response_id +
                                      ",\"result\":" + semantic_tokens_json(semantic_tokens_source(text, uri)) + "}");
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
