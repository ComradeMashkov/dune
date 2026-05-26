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

bool parses_let_and_print() {
    const dune::Program program = parse_source("let x = 40 + 2;\nprint(x);");

    if (!expect(program.statements.size() == 2, "expected two statements")) {
        return false;
    }

    bool passed = true;
    const dune::Statement& let_statement = program.statements[0];
    passed = expect(let_statement.kind == dune::StatementKind::let, "expected let statement") && passed;
    passed = expect(let_statement.name == "x", "expected let variable name") && passed;
    passed =
        expect(let_statement.expression->kind == dune::ExpressionKind::binary, "expected binary expression") && passed;
    passed = expect(let_statement.expression->lexeme == "+", "expected plus expression") && passed;
    passed =
        expect(let_statement.expression->left->kind == dune::ExpressionKind::number, "expected left number") && passed;
    passed = expect(let_statement.expression->left->lexeme == "40", "expected left number lexeme") && passed;
    passed = expect(let_statement.expression->right->kind == dune::ExpressionKind::number, "expected right number") &&
             passed;
    passed = expect(let_statement.expression->right->lexeme == "2", "expected right number lexeme") && passed;

    const dune::Statement& print_statement = program.statements[1];
    passed = expect(print_statement.kind == dune::StatementKind::print, "expected print statement") && passed;
    passed =
        expect(print_statement.expression->kind == dune::ExpressionKind::identifier, "expected print identifier") &&
        passed;
    passed = expect(print_statement.expression->lexeme == "x", "expected print identifier lexeme") && passed;

    return passed;
}

bool parses_operator_precedence() {
    const dune::Program program = parse_source("let value = 1 + 2 * 3;");

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

} // namespace

int main() {
    bool passed = true;
    passed = parses_let_and_print() && passed;
    passed = parses_operator_precedence() && passed;

    return passed ? 0 : 1;
}
