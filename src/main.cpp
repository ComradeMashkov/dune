#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "vm/vm.hpp"

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

int run_file(const std::string& path) {
    const std::string source = read_file(path);

    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::Compiler compiler;
    dune::VirtualMachine vm(compiler.compile(parser.parse()));
    vm.run(std::cout);

    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "dune " << version << '\n';
        return 0;
    }

    if (argc != 2) {
        std::cerr << "usage: dune <file.dn>\n";
        return 1;
    }

    try {
        return run_file(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
