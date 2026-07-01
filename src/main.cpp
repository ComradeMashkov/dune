#include "codegen/llvm_ir_generator.hpp"
#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
#include "lsp/lsp_server.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp"
#include "vm/vm.hpp"

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

#ifndef DUNE_CLANGXX_PATH
#define DUNE_CLANGXX_PATH "clang++"
#endif

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

    void done(std::string_view step) const {
        std::cerr << "  " << style("[done]", "\033[32m") << ' ' << step << '\n';
    }

    void error(std::string_view step, std::string_view message) const {
        std::cerr << "  " << style("[error]", "\033[31m") << ' ' << step << '\n';
        std::cerr << "          " << message << '\n';
    }

private:
    std::string style(const std::string& text, std::string_view ansi) const {
        if (!color_) {
            return text;
        }

        return std::string(ansi) + text + "\033[0m";
    }

    bool color_ = false;
};

class CliReportedError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

template <typename Fn> decltype(auto) run_step(const CliReporter& reporter, std::string_view step, Fn&& fn) {
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
        reporter.error(step, error.what());
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

std::string shell_quote(const std::string& value) {
#if defined(_WIN32)
    std::string quoted = "\"";
    for (const char character : value) {
        if (character == '"') {
            quoted += "\\\"";
        } else {
            quoted += character;
        }
    }

    quoted += "\"";
    return quoted;
#else
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted += character;
        }
    }

    quoted += "'";
    return quoted;
#endif
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

std::string generate_llvm_ir(const dune::Program& program) {
    dune::LlvmIrGenerator generator;
    std::ostringstream output;
    generator.generate(program, output);
    return output.str();
}

dune::Program load_program_with_status(const std::string& source_path, const CliReporter& reporter) {
    const std::filesystem::path source_directory = std::filesystem::path(source_path).parent_path();
    const std::string source = run_step(reporter, "read source", [&] { return read_file(source_path); });
    const std::vector<dune::Token> tokens = run_step(reporter, "lex", [&] { return lex_source(source); });
    dune::Program parsed = run_step(reporter, "parse AST", [&] { return parse_tokens(tokens); });
    return run_step(reporter, "resolve modules", [&] { return resolve_modules(std::move(parsed), source_directory); });
}

void compile_llvm_ir(const std::string& llvm_ir_path, const std::string& output_path) {
    const char* clangxx = std::getenv("DUNE_CLANGXX");
    const std::string compiler = clangxx == nullptr ? DUNE_CLANGXX_PATH : clangxx;
    if (compiler.empty()) {
        throw std::runtime_error("native backend unavailable: dune was built without clang++ "
                                 "(reconfigure with -D DUNE_ENABLE_NATIVE=ON, or set the DUNE_CLANGXX "
                                 "environment variable to a clang++)");
    }
    std::string command = shell_quote(compiler) + " " + shell_quote(llvm_ir_path) + " -o " + shell_quote(output_path);
#if !defined(_WIN32)
    command += " -lm";
#endif
#if defined(_WIN32)
    command = "\"" + command + "\"";
#endif

    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("LLVM backend failed");
    }
}

int run_source_file(const std::string& path, std::vector<std::string> script_arguments) {
    dune::VirtualMachine vm(compile_bytecode(parse_source(read_file(path), std::filesystem::path(path).parent_path())),
                            std::move(script_arguments));
    vm.run(std::cout);

    return 0;
}

int build_native_file(const std::string& source_path, const std::string& output_path) {
    CliReporter reporter("build " + source_path);
    const std::string llvm_ir_path = output_path + ".ll";
    const dune::Program program = load_program_with_status(source_path, reporter);
    run_step(reporter, "type check", [&] { check_program(program); });
    const std::string llvm_ir = run_step(reporter, "emit LLVM IR", [&] { return generate_llvm_ir(program); });
    run_step(reporter, "write LLVM IR", [&] { write_file(llvm_ir_path, llvm_ir); });
    run_step(reporter, "compile native", [&] { compile_llvm_ir(llvm_ir_path, output_path); });
    return 0;
}

int emit_llvm_file(const std::string& source_path, const std::string& output_path) {
    CliReporter reporter("llvm " + source_path);
    const dune::Program program = load_program_with_status(source_path, reporter);
    run_step(reporter, "type check", [&] { check_program(program); });
    const std::string llvm_ir = run_step(reporter, "emit LLVM IR", [&] { return generate_llvm_ir(program); });
    run_step(reporter, "write LLVM IR", [&] { write_file(output_path, llvm_ir); });
    return 0;
}

int check_source_file(const std::string& source_path) {
    CliReporter reporter("check " + source_path);
    const dune::Program program = load_program_with_status(source_path, reporter);
    run_step(reporter, "type check", [&] { check_program(program); });
    return 0;
}

void print_usage() {
    std::cerr << "usage:\n";
    std::cerr << "  dune <file.dn>\n";
    std::cerr << "  dune check <file.dn>\n";
    std::cerr << "  dune lsp\n";
    std::cerr << "  dune build <file.dn> -o <output>\n";
    std::cerr << "  dune llvm <file.dn> -o <file.ll>\n";
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

            if (command == "build" && argc == 5 && std::string(argv[3]) == "-o") {
                return build_native_file(argv[2], argv[4]);
            }

            if (command == "llvm" && argc == 5 && std::string(argv[3]) == "-o") {
                return emit_llvm_file(argv[2], argv[4]);
            }

            // `dune <file.dn> [args...]` runs a script; args are exposed via process.args().
            const bool is_subcommand =
                command == "lsp" || command == "check" || command == "build" || command == "llvm";
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
