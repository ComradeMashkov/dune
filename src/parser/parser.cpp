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

    throw std::runtime_error("expected statement");
}

Statement Parser::let_statement() {
    const Token& name = consume(TokenType::identifier, "expected variable name after let");
    consume(TokenType::equal, "expected '=' after variable name");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::semicolon, "expected ';' after let statement");

    return Statement{StatementKind::let, name.lexeme, std::move(value)};
}

Statement Parser::print_statement() {
    consume(TokenType::left_paren, "expected '(' after print");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::right_paren, "expected ')' after print expression");
    consume(TokenType::semicolon, "expected ';' after print statement");

    return Statement{StatementKind::print, "", std::move(value)};
}

std::unique_ptr<Expression> Parser::expression() {
    return term();
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
