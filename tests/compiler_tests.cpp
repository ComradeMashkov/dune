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

bool compiles_stdlib_primitives() {
    const dune::Bytecode bytecode = compile_source("extern fn c_sqrt(value: real64) -> real64 = \"sqrt\"; "
                                                   "let message: text = \"dune\"; print(message[0]); "
                                                   "print(message[1:3]); "
                                                   "let values: [int] = [1, 2, 3]; let part: [int] = values[:2]; "
                                                   "for let i = 0; i < 3; i = i + 1 { "
                                                   "if i == 1 { continue; } break; } "
                                                   "print(c_sqrt(81.0));");

    bool saw_text_index = false;
    bool saw_slice = false;
    bool saw_backward_jump = false;
    bool saw_extern_call = false;
    for (std::size_t index = 0; index < bytecode.instructions.size(); ++index) {
        const dune::Instruction& instruction = bytecode.instructions[index];
        saw_text_index = saw_text_index || instruction.op == dune::OpCode::load_index;
        saw_slice = saw_slice || instruction.op == dune::OpCode::load_slice;
        saw_extern_call = saw_extern_call || instruction.op == dune::OpCode::call;
        if (instruction.op == dune::OpCode::jump && instruction.operand < index) {
            saw_backward_jump = true;
        }
    }

    bool passed = true;
    passed = expect(bytecode.functions.size() == 1, "expected one extern function") && passed;
    passed = expect(bytecode.functions[0].is_extern, "expected extern function flag") && passed;
    passed = expect(bytecode.functions[0].extern_symbol == "sqrt", "expected extern symbol") && passed;
    passed = expect(saw_text_index, "expected text index instruction") && passed;
    passed = expect(saw_slice, "expected slice instruction") && passed;
    passed = expect(saw_backward_jump, "expected loop backward jump") && passed;
    passed = expect(saw_extern_call, "expected extern call instruction") && passed;
    return passed;
}

bool compiles_generic_functions() {
    const dune::Bytecode bytecode = compile_source("fn identity<T>(value: T) -> T { return value; } "
                                                   "fn twice<T: numeric>(value: T) -> T { return value + value; } "
                                                   "print(identity(42)); print(identity(\"ok\")); print(twice(9));");

    int identity_count = 0;
    int twice_count = 0;
    for (const dune::Bytecode::Function& function : bytecode.functions) {
        if (function.name == "identity" && function.arity == 1) {
            ++identity_count;
        }

        if (function.name == "twice" && function.arity == 1) {
            ++twice_count;
        }
    }

    int call_count = 0;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        if (instruction.op == dune::OpCode::call) {
            ++call_count;
        }
    }

    bool passed = true;
    passed = expect(identity_count == 2, "expected only called identity overloads") && passed;
    passed = expect(twice_count == 1, "expected only called bounded numeric overload") && passed;
    passed = expect(call_count == 3, "expected three generic call instructions") && passed;
    return passed;
}

bool compiles_stdlib_extension_methods() {
    const dune::Bytecode bytecode = compile_source("import array; import text; "
                                                   "let values: [int] = [1, 2, 3]; "
                                                   "print(values.first()); print(values.append(4).last()); "
                                                   "let message: text = \" dune \"; print(message.trim());");

    int call_count = 0;
    bool saw_array_first = false;
    bool saw_array_last = false;
    bool saw_text_trim = false;
    for (const dune::Bytecode::Function& function : bytecode.functions) {
        saw_array_first = saw_array_first || function.name == "array.first";
        saw_array_last = saw_array_last || function.name == "array.last";
        saw_text_trim = saw_text_trim || function.name == "text.trim";
    }

    for (const dune::Instruction& instruction : bytecode.instructions) {
        if (instruction.op == dune::OpCode::call) {
            ++call_count;
        }
    }

    bool passed = true;
    passed = expect(saw_array_first, "expected array.first extension specialization") && passed;
    passed = expect(saw_array_last, "expected array.last extension specialization") && passed;
    passed = expect(saw_text_trim, "expected text.trim extension function") && passed;
    passed = expect(call_count == 4, "expected four stdlib extension method calls") && passed;
    return passed;
}

bool compiles_struct_literals_fields_and_methods() {
    const dune::Bytecode bytecode =
        compile_source("struct Point { x: int, y: int } "
                       "impl Point { fn sum() -> int { return self.x + self.y; } } "
                       "let p: Point = Point { x: 10, y: 20 }; print(p.x); print(p.sum());");

    bool saw_make_record = false;
    bool saw_load_field = false;
    bool saw_method = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_make_record = saw_make_record || instruction.op == dune::OpCode::make_record;
        saw_load_field = saw_load_field || instruction.op == dune::OpCode::load_field;
    }

    for (const dune::Bytecode::Function& function : bytecode.functions) {
        saw_method = saw_method || function.name == "sum";
        for (const dune::Instruction& instruction : function.instructions) {
            saw_load_field = saw_load_field || instruction.op == dune::OpCode::load_field;
        }
    }

    bool passed = true;
    passed = expect(saw_make_record, "expected make_record instruction") && passed;
    passed = expect(saw_load_field, "expected load_field instruction") && passed;
    passed = expect(saw_method, "expected struct method function") && passed;
    return passed;
}

bool compiles_match_expression() {
    const dune::Bytecode bytecode = compile_source("let value = 2; let chosen = match value { "
                                                   "1 => 10, 2 => 20, _ => 30, }; print(chosen);");

    bool saw_equal = false;
    bool saw_false_jump = false;
    bool saw_end_jump = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_equal = saw_equal || instruction.op == dune::OpCode::equal;
        saw_false_jump = saw_false_jump || instruction.op == dune::OpCode::jump_if_false;
        saw_end_jump = saw_end_jump || instruction.op == dune::OpCode::jump;
    }

    bool passed = true;
    passed = expect(saw_equal, "expected match equality checks") && passed;
    passed = expect(saw_false_jump, "expected match false jump") && passed;
    passed = expect(saw_end_jump, "expected match end jump") && passed;
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
    passed = compiles_stdlib_primitives() && passed;
    passed = compiles_generic_functions() && passed;
    passed = compiles_stdlib_extension_methods() && passed;
    passed = compiles_struct_literals_fields_and_methods() && passed;
    passed = compiles_match_expression() && passed;

    return passed ? 0 : 1;
}
