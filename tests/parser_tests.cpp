#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <iostream>
#include <string>

namespace {

dune::Program parse_source(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    return parser.parse();
}

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }

    return true;
}

bool parses_binding_and_print() {
    const dune::Program program = parse_source("x := 40 + 2;\nprint(x);");

    if (!expect(program.statements.size() == 2, "expected two statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& binding_statement = program.statements[0];
    passed = expect(binding_statement.kind == dune::StatementKind::binding, "expected binding statement") && passed;
    passed = expect(binding_statement.name == "x", "expected binding name") && passed;
    passed = expect(binding_statement.expression->kind == dune::ExpressionKind::binary, "expected binary expression") &&
             passed;
    passed = expect(binding_statement.expression->lexeme == "+", "expected plus expression") && passed;
    passed = expect(binding_statement.expression->left->kind == dune::ExpressionKind::number, "expected left number") &&
             passed;
    passed = expect(binding_statement.expression->left->lexeme == "40", "expected left number lexeme") && passed;
    passed =
        expect(binding_statement.expression->right->kind == dune::ExpressionKind::number, "expected right number") &&
        passed;
    passed = expect(binding_statement.expression->right->lexeme == "2", "expected right number lexeme") && passed;

    const dune::Statement& print_statement = program.statements[1];
    passed = expect(print_statement.kind == dune::StatementKind::print, "expected print statement") && passed;
    passed =
        expect(print_statement.expression->kind == dune::ExpressionKind::identifier, "expected print identifier") &&
        passed;
    passed = expect(print_statement.expression->lexeme == "x", "expected print identifier lexeme") && passed;

    return passed;
}

bool parses_operator_precedence() {
    const dune::Program program = parse_source("value := 1 + 2 * 3;");

    if (!expect(program.statements.size() == 1, "expected one statement")) {
        return false;
    }

    bool passed = true;
    const dune::Expression& expression = *program.statements[0].expression;
    passed = expect(expression.kind == dune::ExpressionKind::binary, "expected root binary expression") && passed;
    passed = expect(expression.lexeme == "+", "expected plus at root") && passed;
    passed = expect(expression.left->lexeme == "1", "expected one on left side") && passed;
    passed =
        expect(expression.right->kind == dune::ExpressionKind::binary, "expected nested binary expression") && passed;
    passed = expect(expression.right->lexeme == "*", "expected star on right side") && passed;
    passed = expect(expression.right->left->lexeme == "2", "expected two before star") && passed;
    passed = expect(expression.right->right->lexeme == "3", "expected three after star") && passed;

    return passed;
}

bool parses_control_flow() {
    const dune::Program program = parse_source("x := 3; while x > 0 { x = x - 1; } "
                                               "if x == 0 { print(true); } else { print(false); }");

    if (!expect(program.statements.size() == 3, "expected three statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& while_statement = program.statements[1];
    passed = expect(while_statement.kind == dune::StatementKind::while_statement, "expected while statement") && passed;
    passed =
        expect(while_statement.expression->kind == dune::ExpressionKind::binary, "expected while condition") && passed;
    passed = expect(while_statement.expression->lexeme == ">", "expected greater comparison") && passed;
    passed = expect(while_statement.body.size() == 1, "expected one while body statement") && passed;
    passed =
        expect(while_statement.body[0].kind == dune::StatementKind::assign, "expected assignment in while") && passed;
    passed = expect(while_statement.body[0].name == "x", "expected assignment target") && passed;

    const dune::Statement& if_statement = program.statements[2];
    passed = expect(if_statement.kind == dune::StatementKind::if_statement, "expected if statement") && passed;
    passed = expect(if_statement.expression->lexeme == "==", "expected equality comparison") && passed;
    passed = expect(if_statement.body.size() == 1, "expected one then statement") && passed;
    passed = expect(if_statement.else_body.size() == 1, "expected one else statement") && passed;
    passed =
        expect(if_statement.body[0].expression->kind == dune::ExpressionKind::boolean, "expected boolean literal") &&
        passed;

    return passed;
}

bool parses_functions_and_types() {
    const dune::Program program = parse_source("add(a: int, b: int): int { return a + b; } "
                                               "total: int := add(10, 20); print(total);");

    if (!expect(program.statements.size() == 3, "expected function, binding, and print statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& function = program.statements[0];
    passed = expect(function.kind == dune::StatementKind::function, "expected function declaration") && passed;
    passed = expect(function.name == "add", "expected function name") && passed;
    passed = expect(function.parameters.size() == 2, "expected two parameters") && passed;
    passed = expect(function.parameters[0].name == "a", "expected first parameter name") && passed;
    passed = expect(function.parameters[0].type.has_type, "expected first parameter type") && passed;
    passed =
        expect(function.parameters[0].type.type.kind == dune::ValueType::int_type, "expected int parameter") && passed;
    passed = expect(function.type.has_type, "expected return type") && passed;
    passed = expect(function.type.type.kind == dune::ValueType::int_type, "expected int return type") && passed;
    passed = expect(function.body.size() == 1, "expected one function body statement") && passed;
    passed =
        expect(function.body[0].kind == dune::StatementKind::return_statement, "expected return statement") && passed;
    passed =
        expect(function.body[0].expression->kind == dune::ExpressionKind::binary, "expected return binary") && passed;

    const dune::Statement& binding_statement = program.statements[1];
    passed = expect(binding_statement.type.has_type, "expected binding type annotation") && passed;
    passed =
        expect(binding_statement.expression->kind == dune::ExpressionKind::call, "expected call expression") && passed;
    passed = expect(binding_statement.expression->lexeme == "add", "expected call target") && passed;
    passed = expect(binding_statement.expression->arguments.size() == 2, "expected two call arguments") && passed;

    return passed;
}

bool parses_extended_types() {
    const dune::Program program =
        parse_source("byte: u8 := 255; wide: uint64 := 500; ratio: real := 1.5; mark: glyph := 'z';");

    if (!expect(program.statements.size() == 4, "expected four typed statements")) {
        return false;
    }

    bool passed = true;
    passed = expect(program.statements[0].type.type.kind == dune::ValueType::u8_type, "expected u8 type") && passed;
    passed =
        expect(program.statements[1].type.type.kind == dune::ValueType::u64_type, "expected uint64 alias") && passed;
    passed = expect(program.statements[2].type.type.kind == dune::ValueType::real_type, "expected real type") && passed;
    passed =
        expect(program.statements[2].expression->kind == dune::ExpressionKind::floating, "expected floating literal") &&
        passed;
    passed =
        expect(program.statements[3].type.type.kind == dune::ValueType::glyph_type, "expected glyph type") && passed;
    passed =
        expect(program.statements[3].expression->kind == dune::ExpressionKind::character, "expected glyph literal") &&
        passed;

    return passed;
}

bool parses_standard_types_and_unit_calls() {
    const dune::Program program = parse_source("log(message: text): unit { print(message); return; } "
                                               "noop(): unit { } "
                                               "tiny: i8 := 1; wide: i64 := 2; "
                                               "index: usize := 3; offset: isize := 4; "
                                               "rough: real32 := 1.5; exact: real64 := 2.5; "
                                               "log(\"done\"); noop();");

    if (!expect(program.statements.size() == 10, "expected ten statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& log_function = program.statements[0];
    passed = expect(log_function.kind == dune::StatementKind::function, "expected log function") && passed;
    passed =
        expect(log_function.parameters[0].type.type.kind == dune::ValueType::text_type, "expected text parameter") &&
        passed;
    passed = expect(log_function.type.type.kind == dune::ValueType::unit_type, "expected unit return type") && passed;
    passed =
        expect(log_function.body[1].expression == nullptr, "expected bare return statement in unit function") && passed;

    passed = expect(program.statements[2].type.type.kind == dune::ValueType::i8_type, "expected i8 type") && passed;
    passed = expect(program.statements[3].type.type.kind == dune::ValueType::i64_type, "expected i64 type") && passed;
    passed =
        expect(program.statements[4].type.type.kind == dune::ValueType::usize_type, "expected usize type") && passed;
    passed =
        expect(program.statements[5].type.type.kind == dune::ValueType::isize_type, "expected isize type") && passed;
    passed =
        expect(program.statements[6].type.type.kind == dune::ValueType::real32_type, "expected real32 type") && passed;
    passed =
        expect(program.statements[7].type.type.kind == dune::ValueType::real_type, "expected real64 alias") && passed;
    passed =
        expect(program.statements[8].kind == dune::StatementKind::expression_statement, "expected call statement") &&
        passed;
    passed = expect(program.statements[8].expression->arguments[0]->kind == dune::ExpressionKind::string,
                    "expected text literal argument") &&
             passed;

    return passed;
}

bool parses_arrays_imports_and_module_calls() {
    const dune::Program program = parse_source("import math; values: [int] := [1, math.square(2)]; "
                                               "values.push(9); print(values.len()); print(values[1]);");

    if (!expect(program.statements.size() == 5, "expected import, array binding, calls, and print statements")) {
        return false;
    }

    bool passed = true;
    passed = expect(program.statements[0].kind == dune::StatementKind::import_statement, "expected import statement") &&
             passed;
    passed = expect(program.statements[0].name == "math", "expected math import") && passed;
    const dune::Statement& binding_statement = program.statements[1];
    passed =
        expect(binding_statement.type.type.kind == dune::ValueType::array_type, "expected array annotation") && passed;
    passed = expect(binding_statement.type.type.element != nullptr, "expected array element type") && passed;
    passed =
        expect(binding_statement.type.type.element->kind == dune::ValueType::int_type, "expected int array") && passed;
    passed =
        expect(binding_statement.expression->kind == dune::ExpressionKind::array, "expected array literal") && passed;
    passed = expect(binding_statement.expression->arguments.size() == 2, "expected two array elements") && passed;
    passed = expect(binding_statement.expression->arguments[1]->kind == dune::ExpressionKind::method_call,
                    "expected module method call syntax") &&
             passed;
    passed = expect(binding_statement.expression->arguments[1]->lexeme == "square", "expected square member") && passed;

    const dune::Statement& push_statement = program.statements[2];
    passed = expect(push_statement.kind == dune::StatementKind::expression_statement, "expected push call statement") &&
             passed;
    passed =
        expect(push_statement.expression->kind == dune::ExpressionKind::method_call, "expected array method call") &&
        passed;
    passed = expect(push_statement.expression->lexeme == "push", "expected push method") && passed;

    const dune::Statement& len_statement = program.statements[3];
    passed = expect(len_statement.expression->kind == dune::ExpressionKind::method_call, "expected len method call") &&
             passed;
    passed = expect(len_statement.expression->lexeme == "len", "expected len method") && passed;

    const dune::Statement& print_statement = program.statements[4];
    passed =
        expect(print_statement.expression->kind == dune::ExpressionKind::index, "expected index expression") && passed;
    passed =
        expect(print_statement.expression->left->kind == dune::ExpressionKind::identifier, "expected indexed name") &&
        passed;

    return passed;
}

bool parses_constants_and_module_members() {
    const dune::Program program = parse_source("import math; const tau: real64 = math.PI * 2.0; print(math.PI);");

    if (!expect(program.statements.size() == 3, "expected import, const, and print statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& const_statement = program.statements[1];
    passed = expect(const_statement.kind == dune::StatementKind::const_statement, "expected const statement") && passed;
    passed = expect(const_statement.name == "tau", "expected const name") && passed;
    passed = expect(const_statement.type.type.kind == dune::ValueType::real_type, "expected real64 const") && passed;
    passed =
        expect(const_statement.expression->kind == dune::ExpressionKind::binary, "expected const binary") && passed;
    passed = expect(const_statement.expression->left->kind == dune::ExpressionKind::member,
                    "expected module member on left side") &&
             passed;
    passed = expect(const_statement.expression->left->lexeme == "PI", "expected PI member") && passed;
    passed = expect(const_statement.expression->left->left->kind == dune::ExpressionKind::identifier,
                    "expected module identifier") &&
             passed;
    passed = expect(const_statement.expression->left->left->lexeme == "math", "expected math receiver") && passed;

    const dune::Statement& print_statement = program.statements[2];
    passed =
        expect(print_statement.expression->kind == dune::ExpressionKind::member, "expected print member") && passed;
    passed = expect(print_statement.expression->lexeme == "PI", "expected print PI member") && passed;

    return passed;
}

bool parses_casts_unary_logical_and_methods() {
    const dune::Program program = parse_source("done: bool := !false && true || (17 % 5 == 2); "
                                               "exact: real64 := 17 to real64; "
                                               "values: [int] := [1, 2]; values.pop(); "
                                               "message: text := \"dune\"; print(message.contains(\"du\"));");

    if (!expect(program.statements.size() == 6, "expected operators and methods statements")) {
        return false;
    }

    bool passed = true;
    const dune::Expression& logical = *program.statements[0].expression;
    passed = expect(logical.kind == dune::ExpressionKind::binary, "expected logical binary expression") && passed;
    passed = expect(logical.lexeme == "||", "expected logical or at root") && passed;
    passed = expect(logical.left->kind == dune::ExpressionKind::binary, "expected logical and on left side") && passed;
    passed = expect(logical.left->lexeme == "&&", "expected logical and lexeme") && passed;
    passed = expect(logical.left->left->kind == dune::ExpressionKind::unary, "expected unary not") && passed;
    passed = expect(logical.right->kind == dune::ExpressionKind::binary, "expected equality on right side") && passed;
    passed = expect(logical.right->left->kind == dune::ExpressionKind::binary, "expected modulo expression") && passed;
    passed = expect(logical.right->left->lexeme == "%", "expected modulo lexeme") && passed;

    const dune::Expression& cast = *program.statements[1].expression;
    passed = expect(cast.kind == dune::ExpressionKind::cast, "expected cast expression") && passed;
    passed = expect(cast.type.type.kind == dune::ValueType::real_type, "expected real64 cast target") && passed;

    const dune::Statement& pop_statement = program.statements[3];
    passed = expect(pop_statement.expression->kind == dune::ExpressionKind::method_call, "expected pop method call") &&
             passed;
    passed = expect(pop_statement.expression->lexeme == "pop", "expected pop method") && passed;

    const dune::Statement& print_statement = program.statements[5];
    passed =
        expect(print_statement.expression->kind == dune::ExpressionKind::method_call, "expected text method call") &&
        passed;
    passed = expect(print_statement.expression->lexeme == "contains", "expected contains method") && passed;

    return passed;
}

bool parses_stdlib_primitives() {
    const dune::Program program = parse_source("import text; "
                                               "export foreign c_sqrt(value: real64): real64 = \"sqrt\"; "
                                               "message: text := \"dune\"; print(message[1:3]); print(message[:2]); "
                                               "for i := 0; i < 3; i = i + 1 { "
                                               "if i == 1 { continue; } break; }");

    if (!expect(program.statements.size() == 6, "expected import, foreign, text, prints, and for statements")) {
        return false;
    }

    bool passed = true;
    passed =
        expect(program.statements[0].kind == dune::StatementKind::import_statement, "expected text import") && passed;
    passed = expect(program.statements[0].name == "text", "expected text module name") && passed;

    const dune::Statement& external = program.statements[1];
    passed = expect(external.kind == dune::StatementKind::function, "expected foreign function") && passed;
    passed = expect(external.is_extern, "expected foreign flag") && passed;
    passed = expect(external.exported, "expected export flag") && passed;
    passed = expect(external.extern_symbol == "sqrt", "expected foreign symbol") && passed;

    const dune::Statement& first_print = program.statements[3];
    passed = expect(first_print.expression->kind == dune::ExpressionKind::slice, "expected text slice") && passed;
    passed = expect(first_print.expression->arguments[0] != nullptr, "expected explicit slice start") && passed;
    passed = expect(first_print.expression->arguments[1] != nullptr, "expected explicit slice end") && passed;

    const dune::Statement& second_print = program.statements[4];
    passed = expect(second_print.expression->kind == dune::ExpressionKind::slice, "expected prefix slice") && passed;
    passed = expect(second_print.expression->arguments[0] == nullptr, "expected omitted slice start") && passed;
    passed = expect(second_print.expression->arguments[1] != nullptr, "expected prefix slice end") && passed;

    const dune::Statement& loop = program.statements[5];
    passed = expect(loop.kind == dune::StatementKind::for_statement, "expected for statement") && passed;
    passed = expect(loop.initializer != nullptr, "expected for initializer") && passed;
    passed = expect(loop.increment != nullptr, "expected for increment") && passed;
    passed = expect(loop.body.size() == 2, "expected two for body statements") && passed;
    passed = expect(loop.body[0].kind == dune::StatementKind::if_statement, "expected if in for body") && passed;
    passed =
        expect(loop.body[0].body[0].kind == dune::StatementKind::continue_statement, "expected continue") && passed;
    passed = expect(loop.body[1].kind == dune::StatementKind::break_statement, "expected break") && passed;

    return passed;
}

bool parses_generic_functions() {
    const dune::Program program = parse_source("choose<T, R is real, U is numeric>(left: T, middle: R, right: U): U { "
                                               "return right; } print(choose(\"x\", 1.5, 7));");

    if (!expect(program.statements.size() == 2, "expected generic function and print statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& function = program.statements[0];
    passed = expect(function.kind == dune::StatementKind::function, "expected generic function") && passed;
    passed = expect(function.generic_parameters.size() == 3, "expected three generic parameters") && passed;
    passed = expect(function.generic_parameters[0].name == "T", "expected first generic parameter") && passed;
    passed = expect(function.generic_parameters[0].bound.empty(), "expected unbounded generic parameter") && passed;
    passed = expect(function.generic_parameters[1].name == "R", "expected second generic parameter") && passed;
    passed = expect(function.generic_parameters[1].bound == "real", "expected real generic bound") && passed;
    passed = expect(function.generic_parameters[2].name == "U", "expected third generic parameter") && passed;
    passed = expect(function.generic_parameters[2].bound == "numeric", "expected numeric generic bound") && passed;
    passed = expect(function.parameters[0].type.type.kind == dune::ValueType::generic_type,
                    "expected generic first parameter type") &&
             passed;
    passed = expect(function.parameters[0].type.type.name == "T", "expected first parameter generic name") && passed;
    passed = expect(function.parameters[1].type.type.kind == dune::ValueType::generic_type,
                    "expected generic second parameter type") &&
             passed;
    passed = expect(function.parameters[1].type.type.name == "R", "expected second parameter generic name") && passed;
    passed = expect(function.parameters[2].type.type.kind == dune::ValueType::generic_type,
                    "expected generic third parameter type") &&
             passed;
    passed = expect(function.parameters[2].type.type.name == "U", "expected third parameter generic name") && passed;
    passed = expect(function.type.type.kind == dune::ValueType::generic_type, "expected generic return type") && passed;
    passed = expect(function.type.type.name == "U", "expected return generic name") && passed;

    return passed;
}

bool parses_receiver_methods() {
    const dune::Program program = parse_source("export method<T> [T].first(): T { return this[0]; }");

    if (!expect(program.statements.size() == 1, "expected one method statement")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& statement = program.statements[0];
    passed = expect(statement.kind == dune::StatementKind::method_block, "expected method statement") && passed;
    passed = expect(statement.exported, "expected exported method") && passed;
    passed = expect(statement.type.type.kind == dune::ValueType::array_type, "expected array receiver") && passed;
    passed = expect(statement.type.type.element != nullptr, "expected receiver element") && passed;
    passed = expect(statement.type.type.element->kind == dune::ValueType::generic_type, "expected generic element") &&
             passed;
    passed = expect(statement.body.size() == 1, "expected one method") && passed;
    passed = expect(statement.body[0].kind == dune::StatementKind::function, "expected first method") && passed;
    passed = expect(statement.body[0].name == "first", "expected first method name") && passed;
    passed = expect(statement.body[0].generic_parameters.size() == 1, "expected one method generic") && passed;
    passed = expect(statement.body[0].generic_parameters[0].name == "T", "expected method generic name") && passed;
    passed = expect(statement.body[0].parameters.empty(), "expected receiver omitted from method params") && passed;
    passed =
        expect(statement.body[0].type.type.kind == dune::ValueType::generic_type, "expected generic return") && passed;
    passed = expect(statement.body[0].body[0].expression->kind == dune::ExpressionKind::index,
                    "expected zero-based index expression") &&
             passed;
    passed =
        expect(statement.body[0].body[0].expression->right->lexeme == "0", "expected first index to be zero") && passed;

    return passed;
}

bool parses_records_and_record_literals() {
    const dune::Program program = parse_source("record Point { x: real64, y: real64, "
                                               "sum(): real64 { return this.x + this.y; } } "
                                               "p: Point := Point { x: 1.5, y: 2.5 }; print(p.sum());");

    if (!expect(program.statements.size() == 3, "expected record, binding, and print")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& structure = program.statements[0];
    passed = expect(structure.kind == dune::StatementKind::struct_statement, "expected record statement") && passed;
    passed = expect(structure.name == "Point", "expected record name") && passed;
    passed = expect(structure.parameters.size() == 2, "expected two record fields") && passed;
    passed = expect(structure.parameters[0].name == "x", "expected first field name") && passed;
    passed =
        expect(structure.parameters[0].type.type.kind == dune::ValueType::real_type, "expected real64 field") && passed;
    passed = expect(structure.body.size() == 1, "expected one inline record method") && passed;
    passed = expect(structure.body[0].kind == dune::StatementKind::function, "expected inline method") && passed;
    passed = expect(structure.body[0].name == "sum", "expected inline method name") && passed;

    const dune::Expression& literal = *program.statements[1].expression;
    passed =
        expect(literal.kind == dune::ExpressionKind::struct_literal, "expected record literal expression") && passed;
    passed = expect(literal.lexeme == "Point", "expected Point literal") && passed;
    passed = expect(literal.field_names.size() == 2, "expected two literal field names") && passed;
    passed = expect(literal.field_names[0] == "x", "expected first literal field name") && passed;
    passed =
        expect(literal.arguments[1]->kind == dune::ExpressionKind::floating, "expected floating field value") && passed;

    const dune::Expression& call = *program.statements[2].expression;
    passed = expect(call.kind == dune::ExpressionKind::method_call, "expected record method call") && passed;
    passed = expect(call.lexeme == "sum", "expected method name") && passed;

    return passed;
}

bool parses_generic_records_and_when() {
    const dune::Program program = parse_source("record Box<T> { value: T } "
                                               "box: Box<int> := Box { value: 7 }; "
                                               "chosen := when box.value { is 7 { \"seven\" } is _ { \"other\" } };");

    if (!expect(program.statements.size() == 3, "expected record, binding, and when binding")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& structure = program.statements[0];
    passed =
        expect(structure.kind == dune::StatementKind::struct_statement, "expected generic record statement") && passed;
    passed = expect(structure.generic_parameters.size() == 1, "expected one record generic parameter") && passed;
    passed = expect(structure.generic_parameters[0].name == "T", "expected record generic name") && passed;
    passed = expect(structure.parameters[0].type.type.kind == dune::ValueType::generic_type,
                    "expected generic field type") &&
             passed;

    const dune::Statement& box = program.statements[1];
    passed = expect(box.type.type.kind == dune::ValueType::generic_type, "expected named generic type") && passed;
    passed = expect(box.type.type.name == "Box", "expected Box type name") && passed;
    passed = expect(box.type.type.arguments.size() == 1, "expected one Box type argument") && passed;
    passed =
        expect(box.type.type.arguments[0].kind == dune::ValueType::int_type, "expected int Box argument") && passed;

    const dune::Expression& when_expression = *program.statements[2].expression;
    passed =
        expect(when_expression.kind == dune::ExpressionKind::when_expression, "expected when expression") && passed;
    passed = expect(when_expression.left->kind == dune::ExpressionKind::member, "expected matched member expression") &&
             passed;
    passed = expect(when_expression.arguments.size() == 4, "expected two when arms") && passed;
    passed = expect(when_expression.arguments[0]->kind == dune::ExpressionKind::number, "expected literal pattern") &&
             passed;
    passed =
        expect(when_expression.arguments[2]->kind == dune::ExpressionKind::identifier, "expected wildcard pattern") &&
        passed;
    passed = expect(when_expression.arguments[2]->lexeme == "_", "expected wildcard lexeme") && passed;

    return passed;
}

bool parses_choices_and_variant_when() {
    const dune::Program program = parse_source("export choice Maybe<T> { Present(T), Absent, } "
                                               "value: Maybe<int> := Present(42); "
                                               "chosen := when value { is Present(x) { x } is Absent { 0 } };");

    if (!expect(program.statements.size() == 3, "expected choice, binding, and when binding")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& enumeration = program.statements[0];
    passed = expect(enumeration.kind == dune::StatementKind::enum_statement, "expected choice statement") && passed;
    passed = expect(enumeration.exported, "expected exported choice") && passed;
    passed = expect(enumeration.name == "Maybe", "expected Maybe choice name") && passed;
    passed = expect(enumeration.generic_parameters.size() == 1, "expected one choice generic parameter") && passed;
    passed = expect(enumeration.parameters.size() == 2, "expected two choice variants") && passed;
    passed = expect(enumeration.parameters[0].name == "Present", "expected Present variant") && passed;
    passed = expect(enumeration.parameters[0].type.has_type, "expected Present payload type") && passed;
    passed = expect(enumeration.parameters[0].type.type.kind == dune::ValueType::generic_type,
                    "expected generic variant payload") &&
             passed;
    passed = expect(enumeration.parameters[1].name == "Absent", "expected Absent variant") && passed;
    passed = expect(!enumeration.parameters[1].type.has_type, "expected unit variant") && passed;

    const dune::Statement& binding_statement = program.statements[1];
    passed = expect(binding_statement.type.type.kind == dune::ValueType::generic_type, "expected named choice type") &&
             passed;
    passed = expect(binding_statement.type.type.name == "Maybe", "expected Maybe type name") && passed;
    passed =
        expect(binding_statement.expression->kind == dune::ExpressionKind::call, "expected variant constructor call") &&
        passed;

    const dune::Expression& when_expression = *program.statements[2].expression;
    passed =
        expect(when_expression.kind == dune::ExpressionKind::when_expression, "expected when expression") && passed;
    passed = expect(when_expression.arguments.size() == 4, "expected two when arms") && passed;
    passed =
        expect(when_expression.arguments[0]->kind == dune::ExpressionKind::call, "expected payload variant pattern") &&
        passed;
    passed = expect(when_expression.arguments[0]->lexeme == "Present", "expected Present pattern") && passed;
    passed = expect(when_expression.arguments[0]->arguments.size() == 1, "expected one variant binding") && passed;
    passed = expect(when_expression.arguments[0]->arguments[0]->lexeme == "x", "expected x binding") && passed;
    passed = expect(when_expression.arguments[2]->kind == dune::ExpressionKind::identifier,
                    "expected unit variant pattern") &&
             passed;
    passed = expect(when_expression.arguments[2]->lexeme == "Absent", "expected Absent pattern") && passed;

    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = parses_binding_and_print() && passed;
    passed = parses_operator_precedence() && passed;
    passed = parses_control_flow() && passed;
    passed = parses_functions_and_types() && passed;
    passed = parses_extended_types() && passed;
    passed = parses_standard_types_and_unit_calls() && passed;
    passed = parses_arrays_imports_and_module_calls() && passed;
    passed = parses_constants_and_module_members() && passed;
    passed = parses_casts_unary_logical_and_methods() && passed;
    passed = parses_stdlib_primitives() && passed;
    passed = parses_generic_functions() && passed;
    passed = parses_receiver_methods() && passed;
    passed = parses_records_and_record_literals() && passed;
    passed = parses_generic_records_and_when() && passed;
    passed = parses_choices_and_variant_when() && passed;

    return passed ? 0 : 1;
}
