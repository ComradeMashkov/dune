#include "repl.hpp"

#include "compiler/compiler.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/snippet.hpp"
#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "vm/vm.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dune::repl {

namespace {

std::string_view trim_ascii(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

bool has_open_delimiter(std::string_view source) {
    enum class Mode {
        source,
        string,
        raw_string,
        character,
        line_comment,
        block_comment,
    };

    Mode mode = Mode::source;
    int parentheses = 0;
    int brackets = 0;
    int braces = 0;
    bool escaped = false;

    for (std::size_t index = 0; index < source.size(); ++index) {
        const char current = source[index];
        const char next = index + 1 < source.size() ? source[index + 1] : '\0';

        if (mode == Mode::line_comment) {
            if (current == '\n') {
                mode = Mode::source;
            }
            continue;
        }

        if (mode == Mode::block_comment) {
            if (current == '*' && next == '/') {
                mode = Mode::source;
                ++index;
            }
            continue;
        }

        if (mode == Mode::string || mode == Mode::character) {
            const char closing = mode == Mode::string ? '"' : '\'';
            if (current == '\n') {
                // Dune strings and glyphs cannot span lines. Let the lexer report
                // the unterminated literal instead of waiting for more input.
                mode = Mode::source;
                escaped = false;
                continue;
            }
            if (escaped) {
                escaped = false;
                continue;
            }
            if (current == '\\') {
                escaped = true;
                continue;
            }
            if (current == closing) {
                mode = Mode::source;
            }
            continue;
        }

        if (mode == Mode::raw_string) {
            if (current == '\n') {
                mode = Mode::source;
            } else if (current == '"') {
                mode = Mode::source;
            }
            continue;
        }

        if (current == '/' && next == '/') {
            mode = Mode::line_comment;
            ++index;
        } else if (current == '/' && next == '*') {
            mode = Mode::block_comment;
            ++index;
        } else if (current == 'r' && next == '"') {
            mode = Mode::raw_string;
            ++index;
        } else if (current == '"') {
            mode = Mode::string;
        } else if (current == '\'') {
            mode = Mode::character;
        } else if (current == '(') {
            ++parentheses;
        } else if (current == ')') {
            --parentheses;
        } else if (current == '[') {
            ++brackets;
        } else if (current == ']') {
            --brackets;
        } else if (current == '{') {
            ++braces;
        } else if (current == '}') {
            --braces;
        }
    }

    return mode == Mode::block_comment || parentheses > 0 || brackets > 0 || braces > 0;
}

bool parser_needs_more_input(const std::string& source) {
    if (has_open_delimiter(source)) {
        return true;
    }

    std::vector<Token> tokens;
    try {
        Lexer lexer(source);
        tokens = lexer.tokenize();
        Parser parser(tokens);
        parser.parse();
    } catch (const DiagnosticError& diagnostic) {
        if (tokens.empty() || !diagnostic.diagnostic().has_location) {
            return false;
        }

        const Token& eof = tokens.back();
        const SourceLocation& location = diagnostic.diagnostic().location;
        return eof.type == TokenType::eof && location.line == eof.line && location.column == eof.column &&
               diagnostic.diagnostic().message.starts_with("expected ");
    } catch (const std::exception&) {
        return false;
    }

    return false;
}

std::string terminate_entry(std::string source) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    Parser parser(tokens);
    const Program program = parser.parse();
    if (program.statements.empty() || tokens.size() < 2 || tokens[tokens.size() - 2].type == TokenType::semicolon) {
        return source;
    }

    switch (program.statements.back().kind) {
    case StatementKind::binding:
    case StatementKind::expression_statement:
    case StatementKind::import_statement:
    case StatementKind::module_declaration:
        source += ";\n";
        return source;
    case StatementKind::const_statement:
    case StatementKind::assign:
    case StatementKind::block:
    case StatementKind::if_statement:
    case StatementKind::while_statement:
    case StatementKind::for_statement:
    case StatementKind::for_in_statement:
    case StatementKind::break_statement:
    case StatementKind::continue_statement:
    case StatementKind::function:
    case StatementKind::method_block:
    case StatementKind::struct_statement:
    case StatementKind::enum_statement:
    case StatementKind::contract_statement:
    case StatementKind::type_alias_statement:
    case StatementKind::return_statement:
    case StatementKind::test_block:
        return source;
    }

    return source;
}

Program parse_source(const std::string& source, const std::filesystem::path& source_directory) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    ModuleLoader loader;
    return loader.resolve(parser.parse(), source_directory);
}

std::size_t stable_output_prefix(std::string_view previous, std::string_view current) {
    const std::size_t limit = std::min(previous.size(), current.size());
    std::size_t common = 0;
    while (common < limit && previous[common] == current[common]) {
        ++common;
    }

    if (common < previous.size() && common < current.size() && common > 0 && current[common - 1] != '\n') {
        const std::size_t newline = current.rfind('\n', common - 1);
        common = newline == std::string_view::npos ? 0 : newline + 1;
    }
    return common;
}

void write_new_output(std::ostream& destination, std::string_view previous, std::string_view current) {
    destination << current.substr(stable_output_prefix(previous, current));
}

void print_help(std::ostream& output) {
    output << "Commands:\n";
    output << "  :help   show this help\n";
    output << "  :reset  clear all session definitions and values\n";
    output << "  :quit   exit the REPL\n";
}

void report_diagnostic(std::ostream& error, const DiagnosticError& diagnostic, std::string_view source) {
    const std::string snippet = render_snippet(diagnostic.diagnostic(), source, "<repl>");
    if (!snippet.empty()) {
        error << snippet;
    } else {
        error << "error: " << diagnostic.what() << '\n';
    }
}

} // namespace

int run(std::istream& input, std::ostream& output, std::ostream& error, const Options& options) {
    output << "Dune " << options.version << '\n';
    output << "Type :help for help.\n";

    std::string session_source;
    std::string pending_source;
    std::string previous_output;
    std::string previous_error;

    while (true) {
        if (options.show_prompts) {
            output << (pending_source.empty() ? "> " : "... ");
            output.flush();
        }

        std::string line;
        if (!std::getline(input, line)) {
            if (options.show_prompts) {
                output << '\n';
            }
            if (!trim_ascii(pending_source).empty()) {
                error << "error: incomplete input\n";
                return 1;
            }
            return 0;
        }

        const std::string_view command = trim_ascii(line);
        if (command == ":quit") {
            return 0;
        }
        if (command == ":help") {
            print_help(output);
            continue;
        }
        if (command == ":reset") {
            session_source.clear();
            pending_source.clear();
            previous_output.clear();
            previous_error.clear();
            output << "session reset\n";
            continue;
        }
        if (!command.empty() && command.front() == ':') {
            error << "error: unknown REPL command '" << command << "'\n";
            continue;
        }

        pending_source += line;
        pending_source += '\n';
        if (trim_ascii(pending_source).empty()) {
            pending_source.clear();
            continue;
        }
        if (parser_needs_more_input(pending_source)) {
            continue;
        }

        std::string entry_source = pending_source;
        try {
            entry_source = terminate_entry(std::move(entry_source));
        } catch (const std::exception&) {
            // Evaluation below owns diagnostics. Keep the original entry so the
            // reported source and location match what the user entered.
            entry_source = pending_source;
        }
        const std::string candidate_source = session_source + entry_source;
        std::ostringstream runtime_output;
        std::ostringstream runtime_error;
        std::istringstream runtime_input;
        try {
            Compiler compiler;
            VirtualMachine vm(compiler.compile_repl(parse_source(candidate_source, options.source_directory)));
            vm.run(runtime_output, runtime_error, runtime_input);

            const std::string current_output = runtime_output.str();
            const std::string current_error = runtime_error.str();
            write_new_output(output, previous_output, current_output);
            write_new_output(error, previous_error, current_error);

            session_source = candidate_source;
            previous_output = current_output;
            previous_error = current_error;
        } catch (const DiagnosticError& diagnostic) {
            report_diagnostic(error, diagnostic, candidate_source);
        } catch (const std::exception& runtime_failure) {
            write_new_output(output, previous_output, runtime_output.str());
            write_new_output(error, previous_error, runtime_error.str());
            error << "error: " << runtime_failure.what() << '\n';
        }

        pending_source.clear();
    }
}

} // namespace dune::repl
