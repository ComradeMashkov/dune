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
    const dune::Bytecode bytecode = compile_source("fn add(a: int, b: int): int { return a + b; } "
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
    const dune::Bytecode bytecode = compile_source("fn log(message: text): unit { print(message); } log(\"done\");");

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
    passed = expect(saw_pop, "expected call statement outcome pop") && passed;
    return passed;
}

bool compiles_type_aliases() {
    const dune::Bytecode bytecode =
        compile_source("type Count = int; fn inc(value: Count): Count { return value + 1; } "
                       "record Point { x: Count, fn new(x: Count): Point { return Point { x: x }; } } "
                       "type PointAlias = Point; point: PointAlias = Point.new(inc(4)); print(point.x);");

    bool saw_inc = false;
    bool saw_constructor = false;
    bool saw_field_load = false;
    for (const dune::Bytecode::Function& function : bytecode.functions) {
        if (function.name == "inc") {
            saw_inc = true;
        }
        if (function.name == "Point.new") {
            saw_constructor = true;
        }
    }

    for (const dune::Instruction& instruction : bytecode.instructions) {
        if (instruction.op == dune::OpCode::load_field) {
            saw_field_load = true;
        }
    }

    bool passed = true;
    passed = expect(saw_inc, "expected alias-typed function") && passed;
    passed = expect(saw_constructor, "expected alias-typed constructor") && passed;
    passed = expect(saw_field_load, "expected alias-typed record field load") && passed;
    return passed;
}

bool compiles_formatted_print() {
    const dune::Bytecode bytecode = compile_source("name: text = \"Dune\"; version: int = 1; "
                                                   "message: text = format(\"{} v{}\", name, version); "
                                                   "print(\"{}\", message);");

    bool saw_print_format = false;
    bool saw_format_text = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        if (instruction.op == dune::OpCode::format_text && instruction.operand == 2) {
            saw_format_text = true;
        }
        if (instruction.op == dune::OpCode::print_format && instruction.operand == 1) {
            saw_print_format = true;
        }
    }

    bool passed = true;
    passed = expect(saw_format_text, "expected format_text instruction") && passed;
    passed = expect(saw_print_format, "expected print_format instruction") && passed;
    return passed;
}

bool compiles_arrays_and_module_calls() {
    const dune::Bytecode bytecode = compile_source("import math; "
                                                   "values: [int] = [1, 2]; "
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
    const dune::Bytecode bytecode = compile_source("values: [int] = [1, 2]; values.push(3); "
                                                   "print(values.pop()); values.clear(); print(values.is_empty()); "
                                                   "message: text = \"dune\"; print(message.len()); "
                                                   "print(message.contains(\"un\")); "
                                                   "value: real64 = 17 to real64; "
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

bool compiles_membership_operator() {
    const dune::Bytecode bytecode = compile_source("values: [int] = [1, 2, 3]; print(2 in values); "
                                                   "message: text = \"dune\"; print(\"un\" in message);");

    bool saw_array_contains = false;
    bool saw_text_in = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_array_contains = saw_array_contains || instruction.op == dune::OpCode::array_contains;
        saw_text_in = saw_text_in || instruction.op == dune::OpCode::text_in;
    }

    bool passed = true;
    passed = expect(saw_array_contains, "expected array_contains instruction") && passed;
    passed = expect(saw_text_in, "expected text_in instruction") && passed;
    return passed;
}

bool compiles_stdlib_primitives() {
    const dune::Bytecode bytecode = compile_source("foreign fn c_sqrt(value: real64): real64 = \"sqrt\"; "
                                                   "message: text = \"dune\"; print(message[0]); "
                                                   "print(message[1:3]); "
                                                   "values: [int] = [1, 2, 3]; part: [int] = values[:2]; "
                                                   "for i = 0; i < 3; i = i + 1 { "
                                                   "if i == 1 { continue; } break; } "
                                                   "print(c_sqrt(81.0));");

    bool saw_text_index = false;
    bool saw_slice = false;
    bool saw_backward_jump = false;
    bool saw_foreign_call = false;
    for (std::size_t index = 0; index < bytecode.instructions.size(); ++index) {
        const dune::Instruction& instruction = bytecode.instructions[index];
        saw_text_index = saw_text_index || instruction.op == dune::OpCode::load_index;
        saw_slice = saw_slice || instruction.op == dune::OpCode::load_slice;
        saw_foreign_call = saw_foreign_call || instruction.op == dune::OpCode::call;
        if (instruction.op == dune::OpCode::jump && instruction.operand < index) {
            saw_backward_jump = true;
        }
    }

    bool passed = true;
    passed = expect(bytecode.functions.size() == 1, "expected one foreign function") && passed;
    passed = expect(bytecode.functions[0].is_extern, "expected foreign function flag") && passed;
    passed = expect(bytecode.functions[0].extern_symbol == "sqrt", "expected foreign symbol") && passed;
    passed = expect(saw_text_index, "expected text index instruction") && passed;
    passed = expect(saw_slice, "expected slice instruction") && passed;
    passed = expect(saw_backward_jump, "expected loop backward jump") && passed;
    passed = expect(saw_foreign_call, "expected foreign call instruction") && passed;
    return passed;
}

bool compiles_for_in_arrays_and_ranges() {
    const dune::Bytecode bytecode = compile_source("values: [int] = [1, 2, 3]; total = 0; "
                                                   "for value in values { total = total + value; } "
                                                   "for i in 0..values.len() { total = total + values[i]; } "
                                                   "print(total);");

    bool saw_array_len = false;
    bool saw_load_index = false;
    bool saw_less = false;
    bool saw_backward_jump = false;
    for (std::size_t index = 0; index < bytecode.instructions.size(); ++index) {
        const dune::Instruction& instruction = bytecode.instructions[index];
        saw_array_len = saw_array_len || instruction.op == dune::OpCode::array_len;
        saw_load_index = saw_load_index || instruction.op == dune::OpCode::load_index;
        saw_less = saw_less || instruction.op == dune::OpCode::less;
        if (instruction.op == dune::OpCode::jump && instruction.operand < index) {
            saw_backward_jump = true;
        }
    }

    bool passed = true;
    passed = expect(saw_array_len, "expected for-in array length check") && passed;
    passed = expect(saw_load_index, "expected for-in array element load") && passed;
    passed = expect(saw_less, "expected for-in range comparison") && passed;
    passed = expect(saw_backward_jump, "expected for-in loop backward jump") && passed;
    return passed;
}

bool compiles_generic_functions() {
    const dune::Bytecode bytecode = compile_source("fn identity<T>(value: T): T { return value; } "
                                                   "fn twice<T is numeric>(value: T): T { return value + value; } "
                                                   "print(identity(42)); print(identity(\"done\")); print(twice(9));");

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

bool compiles_stdlib_receiver_methods() {
    const dune::Bytecode bytecode = compile_source("import array; import text; "
                                                   "values: [int] = [1, 2, 3]; "
                                                   "print(values.first()); print(values.append(4).last()); "
                                                   "message: text = \" dune \"; print(message.trim());");

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
    passed = expect(saw_array_first, "expected array.first method specialization") && passed;
    passed = expect(saw_array_last, "expected array.last method specialization") && passed;
    passed = expect(saw_text_trim, "expected text.trim method function") && passed;
    passed = expect(call_count == 4, "expected four stdlib method calls") && passed;
    return passed;
}

bool compiles_record_literals_fields_and_methods() {
    const dune::Bytecode bytecode =
        compile_source("record Point { x: int, y: int, fn sum(): int { return this.x + this.y; } } "
                       "p: Point = Point { x: 10, y: 20 }; print(p.x); print(p.sum());");

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
    passed = expect(saw_method, "expected record method function") && passed;
    return passed;
}

bool compiles_record_field_defaults() {
    const dune::Bytecode bytecode =
        compile_source("record Optimizer { lr: real64 = 0.01, momentum: real64 = 0.0, name: text = \"sgd\" } "
                       "base: Optimizer = Optimizer {}; fast: Optimizer = Optimizer { lr: 0.1 }; "
                       "print(base.lr); print(fast.momentum); print(fast.name);");

    std::size_t make_record_count = 0;
    bool saw_load_field = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        if (instruction.op == dune::OpCode::make_record) {
            ++make_record_count;
        }
        saw_load_field = saw_load_field || instruction.op == dune::OpCode::load_field;
    }

    bool passed = true;
    passed = expect(make_record_count == 2, "expected two record literals with defaults") && passed;
    passed = expect(saw_load_field, "expected defaulted record field loads") && passed;
    return passed;
}

bool compiles_record_constructors() {
    const dune::Bytecode bytecode = compile_source("record Point { x: int, y: int, "
                                                   "fn new(x: int, y: int): Point { return Point { x: x, y: y }; } } "
                                                   "point: Point = Point.new(10, 20); print(point.x);");

    bool saw_constructor = false;
    bool saw_constructor_call = false;
    bool saw_make_record = false;
    for (std::size_t index = 0; index < bytecode.functions.size(); ++index) {
        const dune::Bytecode::Function& function = bytecode.functions[index];
        if (function.name == "Point.new" && function.arity == 2) {
            saw_constructor = true;
            for (const dune::Instruction& instruction : function.instructions) {
                saw_make_record = saw_make_record || instruction.op == dune::OpCode::make_record;
            }
        }

        for (const dune::Instruction& instruction : bytecode.instructions) {
            if (instruction.op == dune::OpCode::call && instruction.operand == index) {
                saw_constructor_call = saw_constructor_call || function.name == "Point.new";
            }
        }
    }

    bool passed = true;
    passed = expect(saw_constructor, "expected constructor function") && passed;
    passed = expect(saw_constructor_call, "expected constructor call") && passed;
    passed = expect(saw_make_record, "expected constructor record literal") && passed;
    return passed;
}

bool compiles_static_associated_record_functions() {
    const dune::Bytecode bytecode =
        compile_source("record Optimizer { lr: real64, momentum: real64, "
                       "static fn default(): Optimizer { return Optimizer { lr: 0.01, momentum: 0.0 }; } "
                       "fn rate(): real64 { return this.lr; } } "
                       "opt: Optimizer = Optimizer.default(); print(opt.rate());");

    bool saw_static_function = false;
    bool saw_static_call = false;
    bool saw_instance_call = false;
    for (std::size_t index = 0; index < bytecode.functions.size(); ++index) {
        const dune::Bytecode::Function& function = bytecode.functions[index];
        saw_static_function = saw_static_function || function.name == "Optimizer.default";
        for (const dune::Instruction& instruction : bytecode.instructions) {
            if (instruction.op == dune::OpCode::call && instruction.operand == index) {
                saw_static_call = saw_static_call || function.name == "Optimizer.default";
                saw_instance_call = saw_instance_call || function.name == "rate";
            }
        }
    }

    bool passed = true;
    passed = expect(saw_static_function, "expected static associated function") && passed;
    passed = expect(saw_static_call, "expected static associated function call") && passed;
    passed = expect(saw_instance_call, "expected instance method call") && passed;
    return passed;
}

bool compiles_assignment_targets() {
    const dune::Bytecode bytecode = compile_source("record Point { x: int, y: int } "
                                                   "values: [int] = [1, 2]; values[1] = 9; "
                                                   "point: Point = Point { x: 1, y: 2 }; point.x = 7; "
                                                   "points: [Point] = [Point { x: 3, y: 4 }]; points[0].y = 11;");

    bool saw_store_index = false;
    bool saw_store_field = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_store_index = saw_store_index || instruction.op == dune::OpCode::store_index;
        saw_store_field = saw_store_field || instruction.op == dune::OpCode::store_field;
    }

    bool passed = true;
    passed = expect(saw_store_index, "expected store_index instruction") && passed;
    passed = expect(saw_store_field, "expected store_field instruction") && passed;
    return passed;
}

bool compiles_when_expression() {
    const dune::Bytecode bytecode = compile_source("value = 2; chosen = when value { "
                                                   "is 1 { 10 } is 2 { 20 } is _ { 30 } }; print(chosen);");

    bool saw_equal = false;
    bool saw_false_jump = false;
    bool saw_end_jump = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_equal = saw_equal || instruction.op == dune::OpCode::equal;
        saw_false_jump = saw_false_jump || instruction.op == dune::OpCode::jump_if_false;
        saw_end_jump = saw_end_jump || instruction.op == dune::OpCode::jump;
    }

    bool passed = true;
    passed = expect(saw_equal, "expected when equality checks") && passed;
    passed = expect(saw_false_jump, "expected when false jump") && passed;
    passed = expect(saw_end_jump, "expected when end jump") && passed;
    return passed;
}

bool compiles_choice_variants_and_when() {
    const dune::Bytecode bytecode = compile_source("choice Maybe { Present(int), Absent, } "
                                                   "value: Maybe = Present(42); "
                                                   "empty: Maybe = Absent; "
                                                   "chosen = when value { is Present(x) { x } is Absent { 0 } }; "
                                                   "print(chosen); "
                                                   "print(when empty { is Present(x) { x } is Absent { 0 } });");

    bool saw_make_variant = false;
    bool saw_make_unit_variant = false;
    bool saw_load_variant_tag = false;
    bool saw_load_variant_payload = false;
    bool saw_false_jump = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_make_variant = saw_make_variant || instruction.op == dune::OpCode::make_variant;
        saw_make_unit_variant = saw_make_unit_variant || instruction.op == dune::OpCode::make_unit_variant;
        saw_load_variant_tag = saw_load_variant_tag || instruction.op == dune::OpCode::load_variant_tag;
        saw_load_variant_payload = saw_load_variant_payload || instruction.op == dune::OpCode::load_variant_payload;
        saw_false_jump = saw_false_jump || instruction.op == dune::OpCode::jump_if_false;
    }

    bool passed = true;
    passed = expect(saw_make_variant, "expected payload variant construction") && passed;
    passed = expect(saw_make_unit_variant, "expected unit variant construction") && passed;
    passed = expect(saw_load_variant_tag, "expected variant tag checks") && passed;
    passed = expect(saw_load_variant_payload, "expected variant payload binding") && passed;
    passed = expect(saw_false_jump, "expected variant when branch") && passed;
    return passed;
}

bool compiles_arrow_style_choice_when() {
    const dune::Bytecode bytecode = compile_source("choice Maybe { Present(int), Absent, } "
                                                   "value: Maybe = Present(42); "
                                                   "empty: Maybe = Absent; "
                                                   "chosen = when value { Present(x) => x; Absent => 0; }; "
                                                   "print(chosen); "
                                                   "print(when empty { Present(x) => x; _ => 7; });");

    bool saw_load_variant_tag = false;
    bool saw_load_variant_payload = false;
    bool saw_false_jump = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_load_variant_tag = saw_load_variant_tag || instruction.op == dune::OpCode::load_variant_tag;
        saw_load_variant_payload = saw_load_variant_payload || instruction.op == dune::OpCode::load_variant_payload;
        saw_false_jump = saw_false_jump || instruction.op == dune::OpCode::jump_if_false;
    }

    bool passed = true;
    passed = expect(saw_load_variant_tag, "expected arrow variant tag checks") && passed;
    passed = expect(saw_load_variant_payload, "expected arrow variant payload binding") && passed;
    passed = expect(saw_false_jump, "expected arrow variant branch") && passed;
    return passed;
}

bool compiles_tuples_and_destructuring() {
    const dune::Bytecode bytecode = compile_source("fn minmax(): (int, int) { return (2, 5); } "
                                                   "(lo, hi) = minmax(); "
                                                   "sum = when (lo, hi) { (left, right) => left + right; }; "
                                                   "print(sum);");

    bool saw_make_tuple = false;
    bool saw_load_tuple_element = false;
    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_make_tuple = saw_make_tuple || instruction.op == dune::OpCode::make_tuple;
        saw_load_tuple_element = saw_load_tuple_element || instruction.op == dune::OpCode::load_tuple_element;
    }
    for (const dune::Bytecode::Function& function : bytecode.functions) {
        for (const dune::Instruction& instruction : function.instructions) {
            saw_make_tuple = saw_make_tuple || instruction.op == dune::OpCode::make_tuple;
            saw_load_tuple_element = saw_load_tuple_element || instruction.op == dune::OpCode::load_tuple_element;
        }
    }

    bool passed = true;
    passed = expect(saw_make_tuple, "expected tuple construction") && passed;
    passed = expect(saw_load_tuple_element, "expected tuple destructuring loads") && passed;
    return passed;
}

bool compiles_autograd_module_as_dune_code() {
    const dune::Bytecode bytecode = compile_source("import autograd; "
                                                   "x = autograd.variable(2.0); "
                                                   "loss = x.mul(3.0).add(x.pow(2.0)); "
                                                   "loss.backward(); print(loss.data);");

    bool saw_variable = false;
    bool saw_backward = false;
    bool saw_call = false;
    bool saw_field_load = false;
    bool saw_make_record = false;
    bool saw_autograd_extern = false;
    for (const dune::Bytecode::Function& function : bytecode.functions) {
        if (function.name.rfind("autograd.", 0) == 0 && function.is_extern) {
            saw_autograd_extern = true;
        }

        saw_variable = saw_variable || (function.name == "autograd.variable" && !function.is_extern);
        saw_backward = saw_backward || (function.name == "autograd.backward" && !function.is_extern);

        for (const dune::Instruction& instruction : function.instructions) {
            saw_field_load = saw_field_load || instruction.op == dune::OpCode::load_field;
            saw_make_record = saw_make_record || instruction.op == dune::OpCode::make_record;
        }
    }

    for (const dune::Instruction& instruction : bytecode.instructions) {
        saw_call = saw_call || instruction.op == dune::OpCode::call;
        saw_field_load = saw_field_load || instruction.op == dune::OpCode::load_field;
        saw_make_record = saw_make_record || instruction.op == dune::OpCode::make_record;
    }

    bool passed = true;
    passed = expect(saw_variable, "expected autograd.variable Dune function") && passed;
    passed = expect(saw_backward, "expected autograd.backward Dune function") && passed;
    passed = expect(!saw_autograd_extern, "expected no autograd foreign functions") && passed;
    passed = expect(saw_call, "expected autograd call instructions") && passed;
    passed = expect(saw_field_load, "expected autograd field access") && passed;
    passed = expect(saw_make_record, "expected autograd record construction") && passed;
    return passed;
}

bool compiles_matrix_module_as_dune_code() {
    const dune::Bytecode bytecode = compile_source("import matrix; "
                                                   "v = matrix.vector([1, 2, 3]); "
                                                   "w = matrix.vector([4, 5, 6]); "
                                                   "print(v.dot(w)); "
                                                   "m = matrix.from_flat(2, 2, [1, 2, 3, 4]); "
                                                   "print(m.transpose().get(1, 0));");

    bool saw_vector = false;
    bool saw_from_flat = false;
    bool saw_dot = false;
    bool saw_make_record = false;
    bool saw_field_load = false;
    bool saw_matrix_extern = false;
    for (const dune::Bytecode::Function& function : bytecode.functions) {
        if (function.name.rfind("matrix.", 0) == 0 && function.is_extern) {
            saw_matrix_extern = true;
        }

        saw_vector = saw_vector || (function.name == "matrix.vector" && !function.is_extern);
        saw_from_flat = saw_from_flat || (function.name == "matrix.from_flat" && !function.is_extern);
        saw_dot = saw_dot || (function.name == "matrix.dot" && !function.is_extern);

        for (const dune::Instruction& instruction : function.instructions) {
            saw_make_record = saw_make_record || instruction.op == dune::OpCode::make_record;
            saw_field_load = saw_field_load || instruction.op == dune::OpCode::load_field;
        }
    }

    bool passed = true;
    passed = expect(saw_vector, "expected matrix.vector Dune function") && passed;
    passed = expect(saw_from_flat, "expected matrix.from_flat Dune function") && passed;
    passed = expect(saw_dot, "expected matrix.dot Dune method function") && passed;
    passed = expect(!saw_matrix_extern, "expected no matrix foreign functions") && passed;
    passed = expect(saw_make_record, "expected matrix record construction") && passed;
    passed = expect(saw_field_load, "expected matrix field access") && passed;
    return passed;
}

// The stdlib is self-hosted in pure Dune: our own modules must not lean on the
// C/C++ FFI. The single sanctioned native primitive is `runtime.panic`
// ("dune_panic"), which aborts execution and cannot be expressed in Dune. This
// guard fails if any other `foreign fn` sneaks into the standard library.
bool stdlib_stays_pure_dune_except_panic() {
    const std::string source = "import math; import random; import matrix; import autograd; "
                               "import dict; import set; import array; import text; "
                               "import collections; import maybe; import outcome; "
                               "import fs; import process; import csv; "
                               "import assert; import runtime; print(0);";

    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    const dune::Program program = loader.resolve(parser.parse());

    bool passed = true;
    bool saw_panic_extern = false;
    for (const dune::Statement& statement : program.statements) {
        if (statement.kind != dune::StatementKind::function || !statement.is_extern) {
            continue;
        }

        if (statement.extern_symbol == "dune_panic") {
            saw_panic_extern = true;
            continue;
        }

        std::cerr << "unexpected stdlib foreign function: " << statement.name << " = \"" << statement.extern_symbol
                  << "\"\n";
        passed = false;
    }

    passed = expect(saw_panic_extern, "expected runtime.panic to remain the only stdlib foreign function") && passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = compiles_function_table_and_call() && passed;
    passed = compiles_unit_call_statement() && passed;
    passed = compiles_type_aliases() && passed;
    passed = compiles_formatted_print() && passed;
    passed = compiles_arrays_and_module_calls() && passed;
    passed = compiles_module_constants() && passed;
    passed = compiles_operators_casts_and_methods() && passed;
    passed = compiles_membership_operator() && passed;
    passed = compiles_stdlib_primitives() && passed;
    passed = compiles_for_in_arrays_and_ranges() && passed;
    passed = compiles_generic_functions() && passed;
    passed = compiles_stdlib_receiver_methods() && passed;
    passed = compiles_record_literals_fields_and_methods() && passed;
    passed = compiles_record_field_defaults() && passed;
    passed = compiles_record_constructors() && passed;
    passed = compiles_static_associated_record_functions() && passed;
    passed = compiles_assignment_targets() && passed;
    passed = compiles_when_expression() && passed;
    passed = compiles_choice_variants_and_when() && passed;
    passed = compiles_arrow_style_choice_when() && passed;
    passed = compiles_tuples_and_destructuring() && passed;
    passed = compiles_autograd_module_as_dune_code() && passed;
    passed = compiles_matrix_module_as_dune_code() && passed;
    passed = stdlib_stays_pure_dune_except_panic() && passed;

    return passed ? 0 : 1;
}
