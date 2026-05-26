#include "codegen/assembly_generator.hpp"
#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "vm/vm.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

constexpr const char* version = "0.1.0";

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
}

dune::Program parse_source(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    return parser.parse();
}

dune::Bytecode compile_bytecode(const dune::Program& program) {
    dune::Compiler compiler;
    return compiler.compile(program);
}

std::string generate_assembly(const dune::Program& program) {
    dune::AssemblyGenerator generator;
    std::ostringstream output;
    generator.generate(program, output);
    return output.str();
}

void assemble_native(const std::string& assembly_path, const std::string& output_path) {
    const char* cxx = std::getenv("CXX");
    const std::string compiler = cxx == nullptr ? "c++" : cxx;
    const std::string command =
        shell_quote(compiler) + " " + shell_quote(assembly_path) + " -o " + shell_quote(output_path);

    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("assembler/linker failed");
    }
}

int run_source_file(const std::string& path) {
    dune::VirtualMachine vm(compile_bytecode(parse_source(read_file(path))));
    vm.run(std::cout);

    return 0;
}

int build_native_file(const std::string& source_path, const std::string& output_path) {
    const std::string assembly_path = output_path + ".s";
    write_file(assembly_path, generate_assembly(parse_source(read_file(source_path))));
    assemble_native(assembly_path, output_path);
    return 0;
}

int emit_assembly_file(const std::string& source_path, const std::string& output_path) {
    write_file(output_path, generate_assembly(parse_source(read_file(source_path))));
    return 0;
}

void print_usage() {
    std::cerr << "usage:\n";
    std::cerr << "  dune <file.dn>\n";
    std::cerr << "  dune build <file.dn> -o <output>\n";
    std::cerr << "  dune asm <file.dn> -o <file.s>\n";
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

        if (argc == 5 && std::string(argv[1]) == "asm" && std::string(argv[3]) == "-o") {
            return emit_assembly_file(argv[2], argv[4]);
        }

        print_usage();
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
