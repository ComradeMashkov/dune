#pragma once

#include "ast/ast.hpp"
#include "lexer/lexer.hpp"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace dune {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Program parse();

private:
    bool is_at_end() const;
    bool check(TokenType type) const;
    bool check_next(TokenType type) const;
    bool match(TokenType type);

    const Token& advance();
    const Token& peek() const;
    const Token& previous() const;
    const Token& consume(TokenType type, std::string_view message);

    Statement statement();
    Statement assignment_statement();
    Statement block_statement();
    Statement function_statement();
    Statement import_statement();
    Statement if_statement();
    Statement let_statement();
    Statement print_statement();
    Statement return_statement();
    Statement while_statement();

    std::vector<Statement> block();
    std::vector<Parameter> parameters();
    std::vector<std::unique_ptr<Expression>> arguments();
    TypeAnnotation optional_type_annotation();
    TypeAnnotation type_annotation();

    std::unique_ptr<Expression> expression();
    std::unique_ptr<Expression> equality();
    std::unique_ptr<Expression> comparison();
    std::unique_ptr<Expression> term();
    std::unique_ptr<Expression> factor();
    std::unique_ptr<Expression> call();
    std::unique_ptr<Expression> primary();

    std::vector<Token> tokens_;
    std::size_t current_ = 0;
};

} // namespace dune
