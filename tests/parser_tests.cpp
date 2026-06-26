#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <iostream>
#include <stdexcept>
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

bool expect_parse_error(const std::string& source, const char* message) {
    try {
        parse_source(source);
    } catch (const std::runtime_error&) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

bool parses_binding_and_print() {
    const dune::Program program = parse_source("x = 40 + 2;\nprint(x);");

    if (!expect(program.statements.size() == 2, "expected two statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& assignment_statement = program.statements[0];
    passed =
        expect(assignment_statement.kind == dune::StatementKind::assign, "expected assignment statement") && passed;
    passed = expect(assignment_statement.name == "x", "expected assignment name") && passed;
    passed =
        expect(assignment_statement.expression->kind == dune::ExpressionKind::binary, "expected binary expression") &&
        passed;
    passed = expect(assignment_statement.expression->lexeme == "+", "expected plus expression") && passed;
    passed =
        expect(assignment_statement.expression->left->kind == dune::ExpressionKind::number, "expected left number") &&
        passed;
    passed = expect(assignment_statement.expression->left->lexeme == "40", "expected left number lexeme") && passed;
    passed =
        expect(assignment_statement.expression->right->kind == dune::ExpressionKind::number, "expected right number") &&
        passed;
    passed = expect(assignment_statement.expression->right->lexeme == "2", "expected right number lexeme") && passed;

    const dune::Statement& print_statement = program.statements[1];
    passed = expect(print_statement.kind == dune::StatementKind::print, "expected print statement") && passed;
    passed =
        expect(print_statement.expression->kind == dune::ExpressionKind::identifier, "expected print identifier") &&
        passed;
    passed = expect(print_statement.expression->lexeme == "x", "expected print identifier lexeme") && passed;

    return passed;
}

bool parses_formatted_print() {
    const dune::Program program = parse_source("name: text = \"Dune\";\nversion: int = 1;\n"
                                               "print(\"{} v{}\", name, version);");

    if (!expect(program.statements.size() == 3, "expected three statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& print_statement = program.statements[2];
    passed = expect(print_statement.kind == dune::StatementKind::print, "expected print statement") && passed;
    passed =
        expect(print_statement.expression->kind == dune::ExpressionKind::string, "expected format literal") && passed;
    passed = expect(print_statement.arguments.size() == 2, "expected two print format arguments") && passed;
    passed = expect(print_statement.arguments[0]->kind == dune::ExpressionKind::identifier,
                    "expected first format argument") &&
             passed;
    passed = expect(print_statement.arguments[0]->lexeme == "name", "expected name argument") && passed;
    passed = expect(print_statement.arguments[1]->lexeme == "version", "expected version argument") && passed;
    return passed;
}

bool parses_operator_precedence() {
    const dune::Program program = parse_source("value = 1 + 2 * 3;");

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

bool parses_membership_operator_precedence() {
    const dune::Program program = parse_source("ok = 1 + 1 in values && enabled;");

    if (!expect(program.statements.size() == 1, "expected one statement")) {
        return false;
    }

    bool passed = true;
    const dune::Expression& expression = *program.statements[0].expression;
    passed = expect(expression.kind == dune::ExpressionKind::binary, "expected root binary expression") && passed;
    passed = expect(expression.lexeme == "&&", "expected logical and at root") && passed;
    passed = expect(expression.left->kind == dune::ExpressionKind::binary, "expected membership expression") && passed;
    passed = expect(expression.left->lexeme == "in", "expected in operator") && passed;
    passed =
        expect(expression.left->left->kind == dune::ExpressionKind::binary, "expected arithmetic left side") && passed;
    passed = expect(expression.left->left->lexeme == "+", "expected addition before membership") && passed;
    passed = expect(expression.left->right->lexeme == "values", "expected membership container") && passed;
    passed = expect(expression.right->lexeme == "enabled", "expected logical right side") && passed;
    return passed;
}

bool parses_control_flow() {
    const dune::Program program = parse_source("x = 3; while x > 0 { x = x - 1; } "
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

bool parses_assignment_targets() {
    const dune::Program program = parse_source("values[1] = 9; grid[0][1] = 8; point.x = 7; users[0].age = 40;");

    if (!expect(program.statements.size() == 4, "expected four assignment statements")) {
        return false;
    }

    bool passed = true;
    const dune::Expression& array_target = *program.statements[0].target;
    passed = expect(program.statements[0].kind == dune::StatementKind::assign, "expected array assignment") && passed;
    passed = expect(array_target.kind == dune::ExpressionKind::index, "expected array index target") && passed;
    passed = expect(array_target.left->lexeme == "values", "expected values target root") && passed;
    passed = expect(array_target.right->lexeme == "1", "expected index target") && passed;

    const dune::Expression& nested_array_target = *program.statements[1].target;
    passed =
        expect(nested_array_target.kind == dune::ExpressionKind::index, "expected nested array index target") && passed;
    passed = expect(nested_array_target.left->kind == dune::ExpressionKind::index,
                    "expected nested array target receiver") &&
             passed;

    const dune::Expression& field_target = *program.statements[2].target;
    passed = expect(field_target.kind == dune::ExpressionKind::member, "expected member target") && passed;
    passed = expect(field_target.left->lexeme == "point", "expected point receiver") && passed;
    passed = expect(field_target.lexeme == "x", "expected x field target") && passed;

    const dune::Expression& nested_field_target = *program.statements[3].target;
    passed =
        expect(nested_field_target.kind == dune::ExpressionKind::member, "expected nested member target") && passed;
    passed =
        expect(nested_field_target.left->kind == dune::ExpressionKind::index, "expected indexed member receiver") &&
        passed;
    passed = expect(nested_field_target.lexeme == "age", "expected nested age field target") && passed;

    return passed;
}

bool parses_functions_and_types() {
    const dune::Program program = parse_source("fn add(a: int, b: int): int { return a + b; } "
                                               "total: int = add(10, 20); print(total);");

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

bool rejects_legacy_function_syntax() {
    return expect_parse_error("add(a: int, b: int): int { return a + b; }",
                              "expected legacy function syntax to be rejected");
}

bool parses_extended_types() {
    const dune::Program program =
        parse_source("byte: u8 = 255; wide: uint64 = 500; ratio: real = 1.5; mark: glyph = 'z';");

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

bool parses_type_aliases() {
    const dune::Program program = parse_source("type Count = int; export type Names = [text]; "
                                               "record Point { x: real64, y: real64 } type PointList = [Point];");

    if (!expect(program.statements.size() == 4, "expected type aliases and record")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& count = program.statements[0];
    passed = expect(count.kind == dune::StatementKind::type_alias_statement, "expected type alias statement") && passed;
    passed = expect(count.name == "Count", "expected Count alias name") && passed;
    passed = expect(count.type.has_type, "expected Count alias target") && passed;
    passed = expect(count.type.type.kind == dune::ValueType::int_type, "expected Count to alias int") && passed;

    const dune::Statement& names = program.statements[1];
    passed = expect(names.kind == dune::StatementKind::type_alias_statement, "expected exported type alias") && passed;
    passed = expect(names.exported, "expected exported type alias flag") && passed;
    passed = expect(names.type.type.kind == dune::ValueType::array_type, "expected array alias target") && passed;
    passed = expect(names.type.type.element != nullptr, "expected array alias element") && passed;
    passed =
        expect(names.type.type.element->kind == dune::ValueType::text_type, "expected text alias element") && passed;

    const dune::Statement& point_list = program.statements[3];
    passed =
        expect(point_list.kind == dune::StatementKind::type_alias_statement, "expected record type alias") && passed;
    passed = expect(point_list.type.type.kind == dune::ValueType::array_type, "expected record array alias") && passed;
    passed = expect(point_list.type.type.element != nullptr, "expected record alias element") && passed;
    passed = expect(point_list.type.type.element->kind == dune::ValueType::generic_type,
                    "expected parsed record alias target as named type") &&
             passed;
    passed = expect(point_list.type.type.element->name == "Point", "expected Point alias element") && passed;

    return passed;
}

bool rejects_generic_type_aliases() {
    return expect_parse_error("type Vec<T> = [T];", "expected generic type aliases to be rejected");
}

bool parses_format_expression_and_numeric_literals() {
    const dune::Program program = parse_source("size = 1_000_000; mask: u64 = 0xffu64; "
                                               "message: text = format(\"{}\", size);");

    if (!expect(program.statements.size() == 3, "expected numeric and format statements")) {
        return false;
    }

    bool passed = true;
    passed = expect(program.statements[0].expression->kind == dune::ExpressionKind::number,
                    "expected decimal numeric literal") &&
             passed;
    passed =
        expect(program.statements[0].expression->lexeme == "1_000_000", "expected decimal separator lexeme") && passed;
    passed = expect(program.statements[1].expression->lexeme == "0xffu64", "expected hex suffix lexeme") && passed;
    const dune::Expression& call = *program.statements[2].expression;
    passed = expect(call.kind == dune::ExpressionKind::call, "expected format call") && passed;
    passed = expect(call.lexeme == "format", "expected format callee") && passed;
    passed = expect(call.arguments.size() == 2, "expected format string and one argument") && passed;
    return passed;
}

bool parses_raw_and_escaped_literals() {
    const dune::Program program = parse_source(
        R"dune(path: text = r"C:\Users\name\data.csv"; line: text = "hello\n"; tab: glyph = '\t'; quote: glyph = '\'';)dune");

    if (!expect(program.statements.size() == 4, "expected raw, text, and glyph literal bindings")) {
        return false;
    }

    bool passed = true;
    passed =
        expect(program.statements[0].expression->kind == dune::ExpressionKind::string, "expected raw text literal") &&
        passed;
    passed = expect(program.statements[0].expression->lexeme == R"(r"C:\Users\name\data.csv")",
                    "expected raw text lexeme") &&
             passed;
    passed = expect(program.statements[1].expression->kind == dune::ExpressionKind::string,
                    "expected escaped text literal") &&
             passed;
    passed =
        expect(program.statements[1].expression->lexeme == R"("hello\n")", "expected escaped text lexeme") && passed;
    passed = expect(program.statements[2].expression->kind == dune::ExpressionKind::character,
                    "expected escaped tab glyph") &&
             passed;
    passed =
        expect(program.statements[2].expression->lexeme == R"('\t')", "expected escaped tab glyph lexeme") && passed;
    passed = expect(program.statements[3].expression->kind == dune::ExpressionKind::character,
                    "expected escaped quote glyph") &&
             passed;
    passed =
        expect(program.statements[3].expression->lexeme == R"('\'')", "expected escaped quote glyph lexeme") && passed;

    return passed;
}

bool parses_standard_types_and_unit_calls() {
    const dune::Program program = parse_source("fn log(message: text): unit { print(message); return; } "
                                               "fn noop(): unit { } "
                                               "tiny: i8 = 1; wide: i64 = 2; "
                                               "index: usize = 3; offset: isize = 4; "
                                               "rough: real32 = 1.5; exact: real64 = 2.5; "
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
    const dune::Program program = parse_source("import math; values: [int] = [1, math.square(2)]; "
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
    const dune::Program program = parse_source("done: bool = !false && true || (17 % 5 == 2); "
                                               "exact: real64 = 17 to real64; "
                                               "values: [int] = [1, 2]; values.pop(); "
                                               "message: text = \"dune\"; print(message.contains(\"du\"));");

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
                                               "export foreign fn c_sqrt(value: real64): real64 = \"sqrt\"; "
                                               "message: text = \"dune\"; print(message[1:3]); print(message[:2]); "
                                               "for i = 0; i < 3; i = i + 1 { "
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

bool parses_for_in_and_ranges() {
    const dune::Program program = parse_source("values: [int] = [1, 2, 3]; "
                                               "for value in values { print(value); } "
                                               "for i in 0..values.len() { print(values[i]); }");

    if (!expect(program.statements.size() == 3, "expected binding and two for-in statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& array_loop = program.statements[1];
    passed =
        expect(array_loop.kind == dune::StatementKind::for_in_statement, "expected array for-in statement") && passed;
    passed = expect(array_loop.name == "value", "expected array loop variable") && passed;
    passed =
        expect(array_loop.expression->kind == dune::ExpressionKind::identifier, "expected array iterable") && passed;
    passed = expect(array_loop.body.size() == 1, "expected array loop body") && passed;

    const dune::Statement& range_loop = program.statements[2];
    passed =
        expect(range_loop.kind == dune::StatementKind::for_in_statement, "expected range for-in statement") && passed;
    passed = expect(range_loop.name == "i", "expected range loop variable") && passed;
    passed = expect(range_loop.expression->kind == dune::ExpressionKind::range, "expected range expression") && passed;
    passed = expect(range_loop.expression->left->lexeme == "0", "expected range start") && passed;
    passed =
        expect(range_loop.expression->right->kind == dune::ExpressionKind::method_call, "expected range end call") &&
        passed;
    passed = expect(range_loop.expression->right->lexeme == "len", "expected len range end") && passed;
    return passed;
}

bool parses_generic_functions() {
    const dune::Program program =
        parse_source("fn choose<T, R is real, U is numeric>(left: T, middle: R, right: U): U { "
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
                                               "fn sum(): real64 { return this.x + this.y; } } "
                                               "p: Point = Point { x: 1.5, y: 2.5 }; print(p.sum());");

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

bool parses_record_field_defaults() {
    const dune::Program program =
        parse_source("record Optimizer { lr: real64 = 0.01, momentum: real64 = 0.0, name: text = \"sgd\" } "
                     "optimizer: Optimizer = Optimizer { lr: 0.1 };");

    if (!expect(program.statements.size() == 2, "expected record and binding")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& record = program.statements[0];
    passed = expect(record.kind == dune::StatementKind::struct_statement, "expected record statement") && passed;
    passed = expect(record.parameters.size() == 3, "expected three record fields") && passed;
    passed = expect(record.parameters[0].default_value != nullptr, "expected lr default") && passed;
    passed = expect(record.parameters[0].default_value->kind == dune::ExpressionKind::floating,
                    "expected floating default") &&
             passed;
    passed = expect(record.parameters[1].default_value != nullptr, "expected momentum default") && passed;
    passed = expect(record.parameters[2].default_value != nullptr, "expected name default") && passed;
    passed =
        expect(record.parameters[2].default_value->kind == dune::ExpressionKind::string, "expected string default") &&
        passed;

    const dune::Expression& literal = *program.statements[1].expression;
    passed =
        expect(literal.kind == dune::ExpressionKind::struct_literal, "expected record literal expression") && passed;
    passed = expect(literal.field_names.size() == 1, "expected one explicit field") && passed;
    passed = expect(literal.field_names[0] == "lr", "expected lr override") && passed;

    return passed;
}

bool parses_generic_records_and_when() {
    const dune::Program program = parse_source("record Box<T> { value: T } "
                                               "box: Box<int> = Box { value: 7 }; "
                                               "chosen = when box.value { is 7 { \"seven\" } is _ { \"other\" } };");

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

bool parses_record_constructors_visibility_and_contracts() {
    const dune::Program program =
        parse_source("export contract Shape { area(): real64; } "
                     "export record Circle with Shape { radius: real64, "
                     "export fn new(radius: real64): Circle { return Circle { radius: radius }; } "
                     "export static fn one(): Circle { return Circle.new(1.0); } "
                     "export fn area(): real64 { return this.radius; } } "
                     "circle: Circle = Circle.new(2.0); one: Circle = Circle.one();");

    if (!expect(program.statements.size() == 4, "expected contract, record, and static bindings")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& contract = program.statements[0];
    passed = expect(contract.kind == dune::StatementKind::contract_statement, "expected contract statement") && passed;
    passed = expect(contract.exported, "expected exported contract") && passed;
    passed = expect(contract.name == "Shape", "expected contract name") && passed;
    passed = expect(contract.body.size() == 1, "expected one contract method") && passed;
    passed = expect(contract.body[0].name == "area", "expected contract method name") && passed;
    passed = expect(contract.body[0].type.type.kind == dune::ValueType::real_type, "expected contract return type") &&
             passed;

    const dune::Statement& record = program.statements[1];
    passed = expect(record.kind == dune::StatementKind::struct_statement, "expected record statement") && passed;
    passed = expect(record.exported, "expected exported record") && passed;
    passed = expect(record.contracts.size() == 1, "expected one contract implementation") && passed;
    passed = expect(record.contracts[0].name == "Shape", "expected Shape contract") && passed;
    passed = expect(record.parameters.size() == 1, "expected one field") && passed;
    passed = expect(!record.parameters[0].exported, "expected private field by default") && passed;
    passed = expect(record.body.size() == 3, "expected constructor, static method, and method") && passed;
    passed = expect(record.body[0].name == "new", "expected constructor name") && passed;
    passed = expect(record.body[0].exported, "expected exported constructor") && passed;
    passed = expect(record.body[1].name == "one", "expected static method name") && passed;
    passed = expect(record.body[1].is_static_record_member, "expected static record method") && passed;
    passed = expect(record.body[1].exported, "expected exported static method") && passed;
    passed = expect(record.body[2].name == "area", "expected method name") && passed;
    passed = expect(!record.body[2].is_static_record_member, "expected instance method") && passed;
    passed = expect(record.body[2].exported, "expected exported method") && passed;

    const dune::Expression& constructor_call = *program.statements[2].expression;
    passed = expect(constructor_call.kind == dune::ExpressionKind::method_call, "expected constructor method call") &&
             passed;
    passed = expect(constructor_call.lexeme == "new", "expected new call") && passed;
    passed =
        expect(constructor_call.left->kind == dune::ExpressionKind::identifier, "expected record receiver") && passed;
    passed = expect(constructor_call.left->lexeme == "Circle", "expected Circle constructor receiver") && passed;

    const dune::Expression& static_call = *program.statements[3].expression;
    passed = expect(static_call.kind == dune::ExpressionKind::method_call, "expected static method call") && passed;
    passed = expect(static_call.lexeme == "one", "expected static method name") && passed;

    return passed;
}

bool parses_choices_and_variant_when() {
    const dune::Program program = parse_source("export choice Maybe<T> { Present(T), Absent, } "
                                               "value: Maybe<int> = Present(42); "
                                               "chosen = when value { is Present(x) { x } is Absent { 0 } };");

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

bool parses_arrow_style_when_patterns() {
    const dune::Program program = parse_source("choice Maybe { Present(int), Absent, } "
                                               "value: Maybe = Present(42); "
                                               "chosen = when value { Present(x) => x; Absent => 0; }; "
                                               "fallback = when value { _ => 7; };");

    if (!expect(program.statements.size() == 4, "expected choice, binding, and two when bindings")) {
        return false;
    }

    bool passed = true;
    const dune::Expression& when_expression = *program.statements[2].expression;
    passed =
        expect(when_expression.kind == dune::ExpressionKind::when_expression, "expected when expression") && passed;
    passed = expect(when_expression.arguments.size() == 4, "expected two arrow when arms") && passed;
    passed =
        expect(when_expression.arguments[0]->kind == dune::ExpressionKind::call, "expected payload variant pattern") &&
        passed;
    passed = expect(when_expression.arguments[0]->lexeme == "Present", "expected Present pattern") && passed;
    passed = expect(when_expression.arguments[0]->arguments.size() == 1, "expected one pattern binding") && passed;
    passed = expect(when_expression.arguments[0]->arguments[0]->lexeme == "x", "expected x binding") && passed;
    passed = expect(when_expression.arguments[2]->kind == dune::ExpressionKind::identifier,
                    "expected unit variant pattern") &&
             passed;
    passed = expect(when_expression.arguments[2]->lexeme == "Absent", "expected Absent pattern") && passed;

    const dune::Expression& fallback = *program.statements[3].expression;
    passed = expect(fallback.arguments.size() == 2, "expected one wildcard arm") && passed;
    passed =
        expect(fallback.arguments[0]->kind == dune::ExpressionKind::identifier, "expected wildcard pattern") && passed;
    passed = expect(fallback.arguments[0]->lexeme == "_", "expected wildcard lexeme") && passed;

    return passed;
}

bool parses_tuples_and_destructuring_patterns() {
    const dune::Program program =
        parse_source("record Point { x: int, y: int } "
                     "fn minmax(values: [int]): (int, int) { return (values[0], values[1]); } "
                     "(lo, hi) = minmax([1, 2]); "
                     "point: Point = Point { x: 3, y: 4 }; "
                     "sum = when point { Point { x, y } => x + y; }; "
                     "pair_sum = when (lo, hi) { (left, right) => left + right; };");

    if (!expect(program.statements.size() == 6, "expected tuple/destructuring program statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& function = program.statements[1];
    passed = expect(function.type.type.kind == dune::ValueType::tuple_type, "expected tuple return type") && passed;
    passed = expect(function.type.type.arguments.size() == 2, "expected two tuple return elements") && passed;
    passed = expect(function.body[0].expression->kind == dune::ExpressionKind::tuple, "expected tuple return value") &&
             passed;

    const dune::Statement& destructuring = program.statements[2];
    passed =
        expect(destructuring.target->kind == dune::ExpressionKind::tuple, "expected tuple assignment target") && passed;
    passed = expect(destructuring.target->arguments.size() == 2, "expected two destructuring bindings") && passed;

    const dune::Expression& record_when = *program.statements[4].expression;
    passed =
        expect(record_when.kind == dune::ExpressionKind::when_expression, "expected record when expression") && passed;
    passed = expect(record_when.arguments[0]->kind == dune::ExpressionKind::struct_literal,
                    "expected record destructuring pattern") &&
             passed;
    passed = expect(record_when.arguments[0]->field_names.size() == 2, "expected two record pattern fields") && passed;

    const dune::Expression& tuple_when = *program.statements[5].expression;
    passed =
        expect(tuple_when.left->kind == dune::ExpressionKind::tuple, "expected tuple subject expression") && passed;
    passed =
        expect(tuple_when.arguments[0]->kind == dune::ExpressionKind::tuple, "expected tuple destructuring pattern") &&
        passed;

    return passed;
}

bool parses_array_comprehensions() {
    const dune::Program program = parse_source("nums = [1, 2, 3];\n"
                                               "squares = [n * n for n in nums];\n"
                                               "evens = [n for n in nums if n > 1];\n");

    if (!expect(program.statements.size() == 3, "expected three statements")) {
        return false;
    }

    bool passed = true;

    const dune::Expression& squares = *program.statements[1].expression;
    passed =
        expect(squares.kind == dune::ExpressionKind::array_comprehension, "expected array comprehension expression") &&
        passed;
    passed = expect(squares.lexeme == "n", "expected comprehension variable 'n'") && passed;
    passed = expect(squares.left != nullptr && squares.left->kind == dune::ExpressionKind::binary,
                    "expected comprehension body to be a binary expression") &&
             passed;
    passed = expect(squares.right != nullptr && squares.right->kind == dune::ExpressionKind::identifier &&
                        squares.right->lexeme == "nums",
                    "expected comprehension iterable to be 'nums'") &&
             passed;
    passed = expect(squares.arguments.empty(), "expected no filter on squares comprehension") && passed;

    const dune::Expression& evens = *program.statements[2].expression;
    passed = expect(evens.kind == dune::ExpressionKind::array_comprehension, "expected filtered array comprehension") &&
             passed;
    passed = expect(evens.arguments.size() == 1, "expected one filter expression on evens comprehension") && passed;
    passed = expect(evens.arguments.front()->kind == dune::ExpressionKind::binary,
                    "expected filter to be a binary comparison") &&
             passed;

    return passed;
}

bool parses_try_operator() {
    const dune::Program program = parse_source("value = parse_value(text)?;\n");
    bool passed = expect(program.statements.size() == 1, "expected one statement");
    const dune::Expression& value = *program.statements[0].expression;
    passed = expect(value.kind == dune::ExpressionKind::try_expression, "expected try expression") && passed;
    passed = expect(value.lexeme == "?", "expected '?' lexeme") && passed;
    passed = expect(value.left != nullptr && value.left->kind == dune::ExpressionKind::call,
                    "expected call operand for '?'") &&
             passed;
    return passed;
}

bool parses_try_operator_binds_tighter_than_binary() {
    const dune::Program program = parse_source("total = 1 + read()?;\n");
    bool passed = expect(program.statements.size() == 1, "expected one statement");
    const dune::Expression& total = *program.statements[0].expression;
    passed = expect(total.kind == dune::ExpressionKind::binary, "expected binary root expression") && passed;
    passed = expect(total.lexeme == "+", "expected '+' at root") && passed;
    passed = expect(total.right != nullptr && total.right->kind == dune::ExpressionKind::try_expression,
                    "expected '?' to bind tighter than '+'") &&
             passed;
    return passed;
}

bool parses_array_literals_without_comprehension() {
    const dune::Program program = parse_source("xs = [1, 2, 3];\n");
    bool passed = expect(program.statements.size() == 1, "expected one statement");
    const dune::Expression& array = *program.statements[0].expression;
    passed = expect(array.kind == dune::ExpressionKind::array, "expected plain array literal") && passed;
    passed = expect(array.arguments.size() == 3, "expected three array elements") && passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = parses_binding_and_print() && passed;
    passed = parses_formatted_print() && passed;
    passed = parses_format_expression_and_numeric_literals() && passed;
    passed = parses_operator_precedence() && passed;
    passed = parses_membership_operator_precedence() && passed;
    passed = parses_control_flow() && passed;
    passed = parses_assignment_targets() && passed;
    passed = parses_functions_and_types() && passed;
    passed = rejects_legacy_function_syntax() && passed;
    passed = parses_extended_types() && passed;
    passed = parses_type_aliases() && passed;
    passed = rejects_generic_type_aliases() && passed;
    passed = parses_raw_and_escaped_literals() && passed;
    passed = parses_standard_types_and_unit_calls() && passed;
    passed = parses_arrays_imports_and_module_calls() && passed;
    passed = parses_constants_and_module_members() && passed;
    passed = parses_casts_unary_logical_and_methods() && passed;
    passed = parses_stdlib_primitives() && passed;
    passed = parses_for_in_and_ranges() && passed;
    passed = parses_generic_functions() && passed;
    passed = parses_receiver_methods() && passed;
    passed = parses_records_and_record_literals() && passed;
    passed = parses_record_field_defaults() && passed;
    passed = parses_generic_records_and_when() && passed;
    passed = parses_record_constructors_visibility_and_contracts() && passed;
    passed = parses_choices_and_variant_when() && passed;
    passed = parses_arrow_style_when_patterns() && passed;
    passed = parses_tuples_and_destructuring_patterns() && passed;
    passed = parses_array_comprehensions() && passed;
    passed = parses_array_literals_without_comprehension() && passed;
    passed = parses_try_operator() && passed;
    passed = parses_try_operator_binds_tighter_than_binary() && passed;

    return passed ? 0 : 1;
}
