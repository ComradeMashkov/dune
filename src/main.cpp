#include "codegen/llvm_ir_generator.hpp"
#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "vm/vm.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* version = "0.8.0";

#ifndef DUNE_CLANGXX_PATH
#define DUNE_CLANGXX_PATH "clang++"
#endif

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

void compile_llvm_ir(const std::string& llvm_ir_path, const std::string& output_path) {
    const char* clangxx = std::getenv("DUNE_CLANGXX");
    const std::string compiler = clangxx == nullptr ? DUNE_CLANGXX_PATH : clangxx;
    std::string command = shell_quote(compiler) + " " + shell_quote(llvm_ir_path) + " -o " + shell_quote(output_path);
#if defined(_WIN32)
    command = "\"" + command + "\"";
#endif

    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("LLVM backend failed");
    }
}

int run_source_file(const std::string& path) {
    dune::VirtualMachine vm(compile_bytecode(parse_source(read_file(path), std::filesystem::path(path).parent_path())));
    vm.run(std::cout);

    return 0;
}

int build_native_file(const std::string& source_path, const std::string& output_path) {
    const std::string llvm_ir_path = output_path + ".ll";
    write_file(llvm_ir_path, generate_llvm_ir(parse_source(read_file(source_path),
                                                           std::filesystem::path(source_path).parent_path())));
    compile_llvm_ir(llvm_ir_path, output_path);
    return 0;
}

int emit_llvm_file(const std::string& source_path, const std::string& output_path) {
    write_file(output_path, generate_llvm_ir(parse_source(read_file(source_path),
                                                          std::filesystem::path(source_path).parent_path())));
    return 0;
}

void print_usage() {
    std::cerr << "usage:\n";
    std::cerr << "  dune <file.dn>\n";
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
        if (argc == 2) {
            return run_source_file(argv[1]);
        }

        if (argc == 5 && std::string(argv[1]) == "build" && std::string(argv[3]) == "-o") {
            return build_native_file(argv[2], argv[4]);
        }

        if (argc == 5 && std::string(argv[1]) == "llvm" && std::string(argv[3]) == "-o") {
            return emit_llvm_file(argv[2], argv[4]);
        }

        print_usage();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
