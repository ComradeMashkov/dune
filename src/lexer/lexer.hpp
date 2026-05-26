#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace dune {

enum class TokenType {
    let,
    print,
    if_keyword,
    else_keyword,
    while_keyword,
    true_keyword,
    false_keyword,
    identifier,
    number,
    plus,
    minus,
    star,
    slash,
    equal,
    equal_equal,
    bang_equal,
    greater,
    greater_equal,
    less,
    less_equal,
    semicolon,
    left_paren,
    right_paren,
    left_brace,
    right_brace,
    eof,
};

struct Token {
    TokenType type;
    std::string lexeme;
};

class Lexer {
public:
    explicit Lexer(std::string source);

    Token next_token();
    std::vector<Token> tokenize();

private:
    bool is_at_end() const;
    char advance();
    bool match(char expected);
    char peek() const;
    void skip_whitespace();

    Token make_token(TokenType type, std::size_t start) const;
    Token identifier(std::size_t start);
    Token number(std::size_t start);

    std::string source_;
    std::size_t current_ = 0;
};

} // namespace dune
