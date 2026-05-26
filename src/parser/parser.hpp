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
    bool match(TokenType type);

    const Token& advance();
    const Token& peek() const;
    const Token& previous() const;
    const Token& consume(TokenType type, std::string_view message);

    Statement statement();
    Statement let_statement();
    Statement print_statement();

    std::unique_ptr<Expression> expression();
    std::unique_ptr<Expression> term();
    std::unique_ptr<Expression> factor();
    std::unique_ptr<Expression> primary();

    std::vector<Token> tokens_;
    std::size_t current_ = 0;
};

} // namespace dune
