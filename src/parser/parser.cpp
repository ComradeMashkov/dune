#include "parser.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace dune {

namespace {

std::unique_ptr<Expression> make_leaf(ExpressionKind kind, std::string lexeme) {
    return std::make_unique<Expression>(Expression{kind, std::move(lexeme), nullptr, nullptr});
}

std::unique_ptr<Expression> make_binary(std::unique_ptr<Expression> left, std::string lexeme,
                                        std::unique_ptr<Expression> right) {
    return std::make_unique<Expression>(
        Expression{ExpressionKind::binary, std::move(lexeme), std::move(left), std::move(right)});
}

} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Program Parser::parse() {
    Program program;

    while (!is_at_end()) {
        program.statements.push_back(statement());
    }

    return program;
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::eof;
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) {
        return type == TokenType::eof;
    }

    return peek().type == type;
}

bool Parser::check_next(TokenType type) const {
    if (current_ + 1 >= tokens_.size()) {
        return false;
    }

    return tokens_[current_ + 1].type == type;
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

const Token& Parser::advance() {
    if (!is_at_end()) {
        ++current_;
    }

    return previous();
}

const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

const Token& Parser::consume(TokenType type, std::string_view message) {
    if (check(type)) {
        return advance();
    }

    throw std::runtime_error(std::string(message));
}

Statement Parser::statement() {
    if (match(TokenType::let)) {
        return let_statement();
    }

    if (match(TokenType::print)) {
        return print_statement();
    }

    if (match(TokenType::if_keyword)) {
        return if_statement();
    }

    if (match(TokenType::while_keyword)) {
        return while_statement();
    }

    if (match(TokenType::left_brace)) {
        return block_statement();
    }

    if (check(TokenType::identifier) && check_next(TokenType::equal)) {
        return assignment_statement();
    }

    throw std::runtime_error("expected statement");
}

Statement Parser::assignment_statement() {
    const Token& name = consume(TokenType::identifier, "expected assignment target");
    consume(TokenType::equal, "expected '=' after assignment target");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::semicolon, "expected ';' after assignment");

    return Statement{StatementKind::assign, name.lexeme, std::move(value), {}, {}};
}

Statement Parser::block_statement() {
    return Statement{StatementKind::block, "", nullptr, block(), {}};
}

Statement Parser::if_statement() {
    std::unique_ptr<Expression> condition = expression();
    consume(TokenType::left_brace, "expected '{' before if body");
    std::vector<Statement> then_body = block();
    std::vector<Statement> else_body;

    if (match(TokenType::else_keyword)) {
        consume(TokenType::left_brace, "expected '{' before else body");
        else_body = block();
    }

    return Statement{StatementKind::if_statement, "", std::move(condition), std::move(then_body), std::move(else_body)};
}

Statement Parser::let_statement() {
    const Token& name = consume(TokenType::identifier, "expected variable name after let");
    consume(TokenType::equal, "expected '=' after variable name");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::semicolon, "expected ';' after let statement");

    return Statement{StatementKind::let, name.lexeme, std::move(value), {}, {}};
}

Statement Parser::print_statement() {
    consume(TokenType::left_paren, "expected '(' after print");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::right_paren, "expected ')' after print expression");
    consume(TokenType::semicolon, "expected ';' after print statement");

    return Statement{StatementKind::print, "", std::move(value), {}, {}};
}

Statement Parser::while_statement() {
    std::unique_ptr<Expression> condition = expression();
    consume(TokenType::left_brace, "expected '{' before while body");
    return Statement{StatementKind::while_statement, "", std::move(condition), block(), {}};
}

std::vector<Statement> Parser::block() {
    std::vector<Statement> statements;

    while (!check(TokenType::right_brace) && !is_at_end()) {
        statements.push_back(statement());
    }

    consume(TokenType::right_brace, "expected '}' after block");
    return statements;
}

std::unique_ptr<Expression> Parser::expression() {
    return equality();
}

std::unique_ptr<Expression> Parser::equality() {
    std::unique_ptr<Expression> expr = comparison();

    while (match(TokenType::equal_equal) || match(TokenType::bang_equal)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = comparison();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::comparison() {
    std::unique_ptr<Expression> expr = term();

    while (match(TokenType::greater) || match(TokenType::greater_equal) || match(TokenType::less) ||
           match(TokenType::less_equal)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = term();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::term() {
    std::unique_ptr<Expression> expr = factor();

    while (match(TokenType::plus) || match(TokenType::minus)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = factor();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::factor() {
    std::unique_ptr<Expression> expr = primary();

    while (match(TokenType::star) || match(TokenType::slash)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = primary();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::primary() {
    if (match(TokenType::number)) {
        return make_leaf(ExpressionKind::number, previous().lexeme);
    }

    if (match(TokenType::true_keyword) || match(TokenType::false_keyword)) {
        return make_leaf(ExpressionKind::boolean, previous().lexeme);
    }

    if (match(TokenType::identifier)) {
        return make_leaf(ExpressionKind::identifier, previous().lexeme);
    }

    if (match(TokenType::left_paren)) {
        std::unique_ptr<Expression> expr = expression();
        consume(TokenType::right_paren, "expected ')' after expression");
        return expr;
    }

    throw std::runtime_error("expected expression");
}

} // namespace dune
