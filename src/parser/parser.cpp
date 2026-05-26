#include "parser.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace dune {

namespace {

SourceLocation location_from_token(const Token& token) {
    return SourceLocation{token.line, token.column};
}

std::unique_ptr<Expression> make_leaf(ExpressionKind kind, std::string lexeme, SourceLocation location) {
    Expression expression{kind, std::move(lexeme), nullptr, nullptr};
    expression.location = location;
    return std::make_unique<Expression>(std::move(expression));
}

std::unique_ptr<Expression> make_binary(std::unique_ptr<Expression> left, std::string lexeme,
                                        std::unique_ptr<Expression> right, SourceLocation location) {
    Expression expression{ExpressionKind::binary, std::move(lexeme), std::move(left), std::move(right)};
    expression.location = location;
    return std::make_unique<Expression>(std::move(expression));
}

std::unique_ptr<Expression> make_call(std::string name, std::vector<std::unique_ptr<Expression>> arguments,
                                      SourceLocation location) {
    auto expression = std::make_unique<Expression>(Expression{ExpressionKind::call, std::move(name), nullptr, nullptr});
    expression->arguments = std::move(arguments);
    expression->location = location;
    return expression;
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
    if (match(TokenType::fn_keyword)) {
        return function_statement();
    }

    if (match(TokenType::let)) {
        return let_statement();
    }

    if (match(TokenType::return_keyword)) {
        return return_statement();
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

    std::unique_ptr<Expression> value = expression();
    consume(TokenType::semicolon, "expected ';' after expression statement");

    Statement statement{StatementKind::expression_statement, "", std::move(value), {}, {}};
    statement.location = statement.expression->location;
    return statement;
}

Statement Parser::assignment_statement() {
    const Token& name = consume(TokenType::identifier, "expected assignment target");
    consume(TokenType::equal, "expected '=' after assignment target");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::semicolon, "expected ';' after assignment");

    Statement statement{StatementKind::assign, name.lexeme, std::move(value), {}, {}};
    statement.location = location_from_token(name);
    return statement;
}

Statement Parser::block_statement() {
    const Token& brace = previous();
    Statement statement{StatementKind::block, "", nullptr, block(), {}};
    statement.location = location_from_token(brace);
    return statement;
}

Statement Parser::function_statement() {
    const Token& keyword = previous();
    const Token& name = consume(TokenType::identifier, "expected function name after fn");
    consume(TokenType::left_paren, "expected '(' after function name");
    std::vector<Parameter> parsed_parameters = parameters();
    consume(TokenType::right_paren, "expected ')' after function parameters");

    TypeAnnotation return_type;
    if (match(TokenType::arrow)) {
        return_type = type_annotation();
    }

    consume(TokenType::left_brace, "expected '{' before function body");

    Statement statement{StatementKind::function, name.lexeme, nullptr, block(), {}};
    statement.type = return_type;
    statement.parameters = std::move(parsed_parameters);
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::if_statement() {
    const Token& keyword = previous();
    std::unique_ptr<Expression> condition = expression();
    consume(TokenType::left_brace, "expected '{' before if body");
    std::vector<Statement> then_body = block();
    std::vector<Statement> else_body;

    if (match(TokenType::else_keyword)) {
        consume(TokenType::left_brace, "expected '{' before else body");
        else_body = block();
    }

    Statement statement{StatementKind::if_statement, "", std::move(condition), std::move(then_body),
                        std::move(else_body)};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::let_statement() {
    const Token& keyword = previous();
    const Token& name = consume(TokenType::identifier, "expected variable name after let");
    TypeAnnotation declared_type = optional_type_annotation();
    consume(TokenType::equal, "expected '=' after variable name");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::semicolon, "expected ';' after let statement");

    Statement statement{StatementKind::let, name.lexeme, std::move(value), {}, {}};
    statement.type = declared_type;
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::print_statement() {
    const Token& keyword = previous();
    consume(TokenType::left_paren, "expected '(' after print");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::right_paren, "expected ')' after print expression");
    consume(TokenType::semicolon, "expected ';' after print statement");

    Statement statement{StatementKind::print, "", std::move(value), {}, {}};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::return_statement() {
    const Token& keyword = previous();
    std::unique_ptr<Expression> value;
    if (!check(TokenType::semicolon)) {
        value = expression();
    }
    consume(TokenType::semicolon, "expected ';' after return value");

    Statement statement{StatementKind::return_statement, "", std::move(value), {}, {}};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::while_statement() {
    const Token& keyword = previous();
    std::unique_ptr<Expression> condition = expression();
    consume(TokenType::left_brace, "expected '{' before while body");
    Statement statement{StatementKind::while_statement, "", std::move(condition), block(), {}};
    statement.location = location_from_token(keyword);
    return statement;
}

std::vector<Statement> Parser::block() {
    std::vector<Statement> statements;

    while (!check(TokenType::right_brace) && !is_at_end()) {
        statements.push_back(statement());
    }

    consume(TokenType::right_brace, "expected '}' after block");
    return statements;
}

std::vector<Parameter> Parser::parameters() {
    std::vector<Parameter> parsed_parameters;

    if (check(TokenType::right_paren)) {
        return parsed_parameters;
    }

    while (true) {
        const Token& name = consume(TokenType::identifier, "expected parameter name");
        parsed_parameters.push_back(Parameter{name.lexeme, optional_type_annotation(), location_from_token(name)});

        if (!match(TokenType::comma)) {
            break;
        }
    }

    return parsed_parameters;
}

std::vector<std::unique_ptr<Expression>> Parser::arguments() {
    std::vector<std::unique_ptr<Expression>> parsed_arguments;

    if (check(TokenType::right_paren)) {
        return parsed_arguments;
    }

    while (true) {
        parsed_arguments.push_back(expression());

        if (!match(TokenType::comma)) {
            break;
        }
    }

    return parsed_arguments;
}

TypeAnnotation Parser::optional_type_annotation() {
    if (!match(TokenType::colon)) {
        return TypeAnnotation{};
    }

    return type_annotation();
}

TypeAnnotation Parser::type_annotation() {
    if (match(TokenType::int_keyword)) {
        return TypeAnnotation{true, ValueType::int_type};
    }

    if (match(TokenType::bool_keyword)) {
        return TypeAnnotation{true, ValueType::bool_type};
    }

    if (match(TokenType::i8_keyword)) {
        return TypeAnnotation{true, ValueType::i8_type};
    }

    if (match(TokenType::i16_keyword)) {
        return TypeAnnotation{true, ValueType::i16_type};
    }

    if (match(TokenType::i32_keyword)) {
        return TypeAnnotation{true, ValueType::i32_type};
    }

    if (match(TokenType::i64_keyword)) {
        return TypeAnnotation{true, ValueType::i64_type};
    }

    if (match(TokenType::isize_keyword)) {
        return TypeAnnotation{true, ValueType::isize_type};
    }

    if (match(TokenType::u8_keyword) || match(TokenType::uint8_keyword)) {
        return TypeAnnotation{true, ValueType::u8_type};
    }

    if (match(TokenType::u16_keyword) || match(TokenType::uint16_keyword)) {
        return TypeAnnotation{true, ValueType::u16_type};
    }

    if (match(TokenType::u32_keyword) || match(TokenType::uint32_keyword)) {
        return TypeAnnotation{true, ValueType::u32_type};
    }

    if (match(TokenType::u64_keyword) || match(TokenType::uint64_keyword)) {
        return TypeAnnotation{true, ValueType::u64_type};
    }

    if (match(TokenType::usize_keyword)) {
        return TypeAnnotation{true, ValueType::usize_type};
    }

    if (match(TokenType::real32_keyword)) {
        return TypeAnnotation{true, ValueType::real32_type};
    }

    if (match(TokenType::real64_keyword)) {
        return TypeAnnotation{true, ValueType::real_type};
    }

    if (match(TokenType::real_keyword)) {
        return TypeAnnotation{true, ValueType::real_type};
    }

    if (match(TokenType::glyph_keyword)) {
        return TypeAnnotation{true, ValueType::glyph_type};
    }

    if (match(TokenType::text_keyword)) {
        return TypeAnnotation{true, ValueType::text_type};
    }

    if (match(TokenType::unit_keyword)) {
        return TypeAnnotation{true, ValueType::unit_type};
    }

    throw std::runtime_error("expected type annotation");
}

std::unique_ptr<Expression> Parser::expression() {
    return equality();
}

std::unique_ptr<Expression> Parser::equality() {
    std::unique_ptr<Expression> expr = comparison();

    while (match(TokenType::equal_equal) || match(TokenType::bang_equal)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = comparison();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::comparison() {
    std::unique_ptr<Expression> expr = term();

    while (match(TokenType::greater) || match(TokenType::greater_equal) || match(TokenType::less) ||
           match(TokenType::less_equal)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = term();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::term() {
    std::unique_ptr<Expression> expr = factor();

    while (match(TokenType::plus) || match(TokenType::minus)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = factor();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::factor() {
    std::unique_ptr<Expression> expr = call();

    while (match(TokenType::star) || match(TokenType::slash)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = call();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::call() {
    std::unique_ptr<Expression> expr = primary();

    while (match(TokenType::left_paren)) {
        const SourceLocation location = expr->location;
        if (expr->kind != ExpressionKind::identifier) {
            throw std::runtime_error("expected function name before arguments");
        }

        std::vector<std::unique_ptr<Expression>> parsed_arguments = arguments();
        consume(TokenType::right_paren, "expected ')' after function arguments");
        expr = make_call(expr->lexeme, std::move(parsed_arguments), location);
    }

    return expr;
}

std::unique_ptr<Expression> Parser::primary() {
    if (match(TokenType::number)) {
        return make_leaf(ExpressionKind::number, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::float_number)) {
        return make_leaf(ExpressionKind::floating, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::char_literal)) {
        return make_leaf(ExpressionKind::character, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::string_literal)) {
        return make_leaf(ExpressionKind::string, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::true_keyword) || match(TokenType::false_keyword)) {
        return make_leaf(ExpressionKind::boolean, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::identifier)) {
        return make_leaf(ExpressionKind::identifier, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::left_paren)) {
        std::unique_ptr<Expression> expr = expression();
        consume(TokenType::right_paren, "expected ')' after expression");
        return expr;
    }

    throw std::runtime_error("expected expression");
}

} // namespace dune
