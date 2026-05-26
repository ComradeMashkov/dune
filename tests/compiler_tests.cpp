#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"

#include <iostream>
#include <string>

namespace {

dune::Bytecode compile_source(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::Compiler compiler;
    return compiler.compile(loader.resolve(parser.parse()));
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

bool compiles_unit_call_statement() {
    const dune::Bytecode bytecode = compile_source("fn log(message: text) -> unit { print(message); } log(\"ok\");");

    bool saw_call = false;
    bool saw_pop = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        if (instruction.op == dune::OpCode::call) {
            saw_call = true;
        }

        if (instruction.op == dune::OpCode::pop) {
            saw_pop = true;
        }
    }

    bool passed = true;
    passed = expect(saw_call, "expected unit call instruction") && passed;
    passed = expect(saw_pop, "expected call statement result pop") && passed;
    return passed;
}

bool compiles_arrays_and_module_calls() {
    const dune::Bytecode bytecode = compile_source("import math; "
                                                   "let values: [int] = [1, 2]; "
                                                   "values.push(math.square(3)); "
                                                   "print(values.len()); print(values[2]);");

    bool saw_make_array = false;
    bool saw_array_push = false;
    bool saw_array_len = false;
    bool saw_load_index = false;
    bool saw_module_call = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_make_array = saw_make_array || instruction.op == dune::OpCode::make_array;
        saw_array_push = saw_array_push || instruction.op == dune::OpCode::array_push;
        saw_array_len = saw_array_len || instruction.op == dune::OpCode::array_len;
        saw_load_index = saw_load_index || instruction.op == dune::OpCode::load_index;
        saw_module_call = saw_module_call || instruction.op == dune::OpCode::call;
    }

    bool passed = true;
    passed = expect(saw_make_array, "expected make_array instruction") && passed;
    passed = expect(saw_module_call, "expected imported module function call") && passed;
    passed = expect(saw_array_push, "expected array_push instruction") && passed;
    passed = expect(saw_array_len, "expected array_len instruction") && passed;
    passed = expect(saw_load_index, "expected load_index instruction") && passed;
    return passed;
}

bool compiles_module_constants() {
    const dune::Bytecode bytecode = compile_source("import math; print(math.PI);");

    bool saw_constant_store = false;
    bool saw_member_load = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_constant_store = saw_constant_store || instruction.op == dune::OpCode::store_local;
        saw_member_load = saw_member_load || instruction.op == dune::OpCode::load_local;
    }

    bool passed = true;
    passed = expect(saw_constant_store, "expected module constant store") && passed;
    passed = expect(saw_member_load, "expected module member load") && passed;
    return passed;
}

bool compiles_operators_casts_and_methods() {
    const dune::Bytecode bytecode = compile_source("let values: [int] = [1, 2]; values.push(3); "
                                                   "print(values.pop()); values.clear(); print(values.is_empty()); "
                                                   "let message: text = \"dune\"; print(message.len()); "
                                                   "print(message.contains(\"un\")); "
                                                   "let value: real64 = 17 as real64; "
                                                   "print(!false && (17 % 5 == 2));");

    bool saw_modulo = false;
    bool saw_not = false;
    bool saw_cast = false;
    bool saw_array_pop = false;
    bool saw_array_clear = false;
    bool saw_array_is_empty = false;
    bool saw_text_len = false;
    bool saw_text_contains = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_modulo = saw_modulo || instruction.op == dune::OpCode::modulo;
        saw_not = saw_not || instruction.op == dune::OpCode::not_value;
        saw_cast = saw_cast || instruction.op == dune::OpCode::cast_real;
        saw_array_pop = saw_array_pop || instruction.op == dune::OpCode::array_pop;
        saw_array_clear = saw_array_clear || instruction.op == dune::OpCode::array_clear;
        saw_array_is_empty = saw_array_is_empty || instruction.op == dune::OpCode::array_is_empty;
        saw_text_len = saw_text_len || instruction.op == dune::OpCode::text_len;
        saw_text_contains = saw_text_contains || instruction.op == dune::OpCode::text_contains;
    }

    bool passed = true;
    passed = expect(saw_modulo, "expected modulo instruction") && passed;
    passed = expect(saw_not, "expected logical not instruction") && passed;
    passed = expect(saw_cast, "expected cast instruction") && passed;
    passed = expect(saw_array_pop, "expected array_pop instruction") && passed;
    passed = expect(saw_array_clear, "expected array_clear instruction") && passed;
    passed = expect(saw_array_is_empty, "expected array_is_empty instruction") && passed;
    passed = expect(saw_text_len, "expected text_len instruction") && passed;
    passed = expect(saw_text_contains, "expected text_contains instruction") && passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = compiles_function_table_and_call() && passed;
    passed = compiles_unit_call_statement() && passed;
    passed = compiles_arrays_and_module_calls() && passed;
    passed = compiles_module_constants() && passed;
    passed = compiles_operators_casts_and_methods() && passed;

    return passed ? 0 : 1;
}
