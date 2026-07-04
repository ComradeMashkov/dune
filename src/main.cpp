#include "compiler/compiler.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/snippet.hpp"
#include "doc/doc_generator.hpp"
#include "lexer/lexer.hpp"
#include "lsp/lsp_server.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp"
#include "vm/vm.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

constexpr const char* version = "0.13.1";

bool stderr_is_terminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

bool use_color() {
    const char* color = std::getenv("DUNE_COLOR");
    if (color != nullptr) {
        const std::string value = color;
        if (value == "always") {
            return true;
        }

        if (value == "never") {
            return false;
        }
    }

    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }

    return stderr_is_terminal();
}

class CliReporter {
public:
    explicit CliReporter(std::string command) : color_(use_color()) {
        std::cerr << style("dune " + std::move(command), "\033[1m") << '\n';
    }

    // No-op phase marker so CliReporter satisfies the same interface as
    // BuildProgress and can be driven by the shared run_step helper.
    void begin(std::string_view /*step*/) const {}

    void done(std::string_view step) const {
        std::cerr << "  " << style("[done]", "\033[32m") << ' ' << step << '\n';
    }

    // Records the source text + path so a failed step can render a snippet.
    void set_source(std::string source, std::string filename) {
        source_ = std::move(source);
        filename_ = std::move(filename);
        has_source_ = true;
    }

    void error(std::string_view step, const std::exception& error) const {
        std::cerr << "  " << style("[error]", "\033[31m") << ' ' << step << '\n';
        // Module-resolution locations point into the imported file, not the main
        // source we hold, so never quote our source for that step.
        const bool allow_snippet = has_source_ && step != "resolve modules";
        std::cerr << dune::render_error_body(error, source_, filename_, allow_snippet);
    }

private:
    std::string style(const std::string& text, std::string_view ansi) const {
        if (!color_) {
            return text;
        }

        return std::string(ansi) + text + "\033[0m";
    }

    bool color_ = false;
    std::string source_;
    std::string filename_;
    bool has_source_ = false;
};

class CliReportedError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

template <typename Reporter, typename Fn> decltype(auto) run_step(Reporter& reporter, std::string_view step, Fn&& fn) {
    reporter.begin(step);
    try {
        if constexpr (std::is_void_v<std::invoke_result_t<Fn&>>) {
            std::invoke(std::forward<Fn>(fn));
            reporter.done(step);
            return;
        } else {
            auto result = std::invoke(std::forward<Fn>(fn));
            reporter.done(step);
            return result;
        }
    } catch (const std::exception& error) {
        reporter.error(step, error);
        throw CliReportedError(error.what());
    }
}

std::string read_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open '" + path + "'");
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not open '" + path + "' for writing");
    }

    output << content;
}

dune::Program parse_source(const std::string& source, const std::filesystem::path& source_directory) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    return loader.resolve(parser.parse(), source_directory);
}

std::vector<dune::Token> lex_source(const std::string& source) {
    dune::Lexer lexer(source);
    return lexer.tokenize();
}

dune::Program parse_tokens(const std::vector<dune::Token>& tokens) {
    dune::Parser parser(tokens);
    return parser.parse();
}

dune::Program resolve_modules(dune::Program program, const std::filesystem::path& source_directory) {
    dune::ModuleLoader loader;
    return loader.resolve(std::move(program), source_directory);
}

void check_program(const dune::Program& program) {
    dune::TypeChecker checker;
    checker.check(program);
}

dune::Bytecode compile_bytecode(const dune::Program& program) {
    dune::Compiler compiler;
    return compiler.compile(program);
}

template <typename Reporter>
dune::Program load_program_with_status(const std::string& source_path, Reporter& reporter) {
    const std::filesystem::path source_directory = std::filesystem::path(source_path).parent_path();
    const std::string source = run_step(reporter, "read source", [&] { return read_file(source_path); });
    reporter.set_source(source, source_path);
    const std::vector<dune::Token> tokens = run_step(reporter, "lex", [&] { return lex_source(source); });
    dune::Program parsed = run_step(reporter, "parse AST", [&] { return parse_tokens(tokens); });
    return run_step(reporter, "resolve modules", [&] { return resolve_modules(std::move(parsed), source_directory); });
}

// Prints a compile-time diagnostic for the plain (non-status) subcommands: a
// Rust-style source snippet when the error carries a location inside `source`,
// otherwise the familiar `error: <message>` line.
void report_diagnostic(const dune::DiagnosticError& error, std::string_view source, std::string_view filename) {
    const std::string snippet = dune::render_snippet(error.diagnostic(), source, filename);
    if (!snippet.empty()) {
        std::cerr << snippet;
    } else {
        std::cerr << "error: " << error.what() << '\n';
    }
}

int run_source_file(const std::string& path, std::vector<std::string> script_arguments) {
    const std::string source = read_file(path);
    try {
        dune::VirtualMachine vm(compile_bytecode(parse_source(source, std::filesystem::path(path).parent_path())),
                                std::move(script_arguments));
        vm.run(std::cout);
    } catch (const dune::DiagnosticError& error) {
        report_diagnostic(error, source, path);
        return 1;
    }

    return 0;
}

// Runs every `test "..." { ... }` block in a file. Each test runs in isolation;
// a failed assertion aborts that test (via runtime.panic → a thrown exception),
// which is caught and reported without stopping the rest. Exits non-zero if any
// test fails. Top-level executable code is not run — only the test blocks.
int run_test_file(const std::string& path) {
    const std::string source = read_file(path);
    dune::Bytecode bytecode;
    try {
        bytecode = compile_bytecode(parse_source(source, std::filesystem::path(path).parent_path()));
    } catch (const dune::DiagnosticError& error) {
        report_diagnostic(error, source, path);
        return 1;
    }
    const std::vector<dune::Bytecode::Test> tests(bytecode.tests);
    dune::VirtualMachine vm(std::move(bytecode));

    std::cout << "running " << tests.size() << (tests.size() == 1 ? " test\n" : " tests\n");
    std::size_t passed = 0;
    std::size_t failed = 0;
    for (const dune::Bytecode::Test& test : tests) {
        try {
            vm.run_test(test.function_index, std::cout);
            std::cout << "test \"" << test.name << "\" ... ok\n";
            ++passed;
        } catch (const std::exception& error) {
            std::cout << "test \"" << test.name << "\" ... FAILED\n";
            std::cout << "    " << error.what() << '\n';
            ++failed;
        }
    }

    std::cout << "\ntest result: " << (failed == 0 ? "ok" : "FAILED") << ". " << passed << " passed; " << failed
              << " failed\n";
    return failed == 0 ? 0 : 1;
}

int check_source_file(const std::string& source_path) {
    CliReporter reporter("check " + source_path);
    const dune::Program program = load_program_with_status(source_path, reporter);
    run_step(reporter, "type check", [&] { check_program(program); });
    return 0;
}

// Renders one module's Markdown to the given stream.
std::string render_module_file(const std::string& source_path) {
    const std::string source = read_file(source_path);
    const std::string module_name = std::filesystem::path(source_path).stem().string();
    return dune::doc::render_module(source, module_name);
}

// `dune doc <path> [-o <out>] [--check]`.
//
// `<path>` is a single `.dn` file or a directory of them. With no `-o` a single
// file's Markdown is printed to stdout; with `-o` it is written to that path (a
// directory input writes one `<module>.md` per file plus an `index.md`).
// `--check` regenerates in memory and compares against the existing files,
// exiting non-zero on any drift — for CI that keeps generated docs current.
int run_doc(const std::vector<std::string>& arguments) {
    namespace fs = std::filesystem;

    std::string input;
    std::string output;
    bool check = false;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "-o" || argument == "--out") {
            if (index + 1 >= arguments.size()) {
                throw std::runtime_error("dune doc: '" + argument + "' needs a path");
            }
            output = arguments[++index];
        } else if (argument == "--check") {
            check = true;
        } else if (input.empty()) {
            input = argument;
        } else {
            throw std::runtime_error("dune doc: unexpected argument '" + argument + "'");
        }
    }

    if (input.empty()) {
        throw std::runtime_error("dune doc: missing input path");
    }

    if (!fs::is_directory(input)) {
        const std::string markdown = render_module_file(input);
        if (check) {
            const std::string existing = fs::exists(output) ? read_file(output) : std::string{};
            if (existing != markdown) {
                std::cerr << "doc drift: " << output << '\n';
                return 1;
            }
            return 0;
        }
        if (output.empty()) {
            std::cout << markdown;
        } else {
            write_file(output, markdown);
        }
        return 0;
    }

    if (output.empty()) {
        throw std::runtime_error("dune doc: a directory input needs -o <out-dir>");
    }

    std::vector<std::string> stems;
    for (const auto& entry : fs::directory_iterator(input)) {
        if (entry.is_regular_file() && entry.path().extension() == ".dn") {
            stems.push_back(entry.path().stem().string());
        }
    }
    std::sort(stems.begin(), stems.end());

    bool drift = false;
    for (const std::string& stem : stems) {
        const std::string source = read_file((fs::path(input) / (stem + ".dn")).string());
        const std::string markdown = dune::doc::render_module(source, stem);
        const std::string page = (fs::path(output) / (stem + ".md")).string();
        if (check) {
            const std::string existing = fs::exists(page) ? read_file(page) : std::string{};
            if (existing != markdown) {
                std::cerr << "doc drift: " << page << '\n';
                drift = true;
            }
        } else {
            fs::create_directories(output);
            write_file(page, markdown);
        }
    }

    std::string index = "# Standard library reference\n\n";
    for (const std::string& stem : stems) {
        index += "- [`";
        index += stem;
        index += "`](";
        index += stem;
        index += ".md)\n";
    }
    const std::string index_path = (fs::path(output) / "index.md").string();
    if (check) {
        const std::string existing = fs::exists(index_path) ? read_file(index_path) : std::string{};
        if (existing != index) {
            std::cerr << "doc drift: " << index_path << '\n';
            drift = true;
        }
        return drift ? 1 : 0;
    }
    write_file(index_path, index);
    return 0;
}

void print_usage() {
    std::cerr << "usage:\n";
    std::cerr << "  dune <file.dn>\n";
    std::cerr << "  dune check <file.dn>\n";
    std::cerr << "  dune lsp\n";
    std::cerr << "  dune doc <file.dn|dir> [-o <out>] [--check]\n";
    std::cerr << "  dune test <file.dn>\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "dune " << version << '\n';
        return 0;
    }

    try {
        if (argc >= 2) {
            const std::string command = argv[1];

            if (command == "lsp" && argc == 2) {
                return dune::lsp::run(std::cin, std::cout);
            }

            if (command == "check" && argc == 3) {
                return check_source_file(argv[2]);
            }

            if (command == "doc" && argc >= 3) {
                return run_doc(std::vector<std::string>(argv + 2, argv + argc));
            }

            if (command == "test" && argc == 3) {
                return run_test_file(argv[2]);
            }

            // `dune <file.dn> [args...]` runs a script; args are exposed via process.args().
            const bool is_subcommand =
                command == "lsp" || command == "check" || command == "doc" || command == "test";
            if (!is_subcommand) {
                return run_source_file(command, std::vector<std::string>(argv + 2, argv + argc));
            }
        }

        print_usage();
        return 1;
    } catch (const CliReportedError&) {
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
