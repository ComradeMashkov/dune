#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <iostream>
#include <string>

namespace {

dune::Bytecode compile_source(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::Compiler compiler;
    return compiler.compile(parser.parse());
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }

    return true;
}

bool compiles_function_table_and_call() {
    const dune::Bytecode bytecode = compile_source("fn add(a: int, b: int) -> int { return a + b; } "
                                                   "print(add(1, 2));");

    bool passed = true;
    passed = expect(bytecode.functions.size() == 1, "expected one compiled function") && passed;
    passed = expect(bytecode.functions[0].name == "add", "expected function name") && passed;
    passed = expect(bytecode.functions[0].arity == 2, "expected function arity") && passed;
    passed = expect(bytecode.functions[0].local_count == 2, "expected parameter locals") && passed;

    bool saw_call = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        if (instruction.op == dune::OpCode::call && instruction.operand == 0) {
            saw_call = true;
        }
    }

    bool saw_return = false;
    for (const dune::Instruction& instruction : bytecode.functions[0].instructions) {
        if (instruction.op == dune::OpCode::return_value) {
            saw_return = true;
        }
    }

    passed = expect(saw_call, "expected call instruction") && passed;
    passed = expect(saw_return, "expected return instruction") && passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = compiles_function_table_and_call() && passed;

    return passed ? 0 : 1;
}
