#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace dune {

enum class TokenType {
    let,
    fn_keyword,
    return_keyword,
    print,
    if_keyword,
    else_keyword,
    while_keyword,
    int_keyword,
    bool_keyword,
    u8_keyword,
    u16_keyword,
    u32_keyword,
    u64_keyword,
    uint8_keyword,
    uint16_keyword,
    uint32_keyword,
    uint64_keyword,
    real_keyword,
    glyph_keyword,
    true_keyword,
    false_keyword,
    identifier,
    number,
    float_number,
    char_literal,
    plus,
    minus,
    arrow,
    star,
    slash,
    equal,
    equal_equal,
    bang_equal,
    greater,
    greater_equal,
    less,
    less_equal,
    colon,
    comma,
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
    std::size_t line = 1;
    std::size_t column = 1;
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

    Token make_token(TokenType type, std::size_t start, std::size_t line, std::size_t column) const;
    Token identifier(std::size_t start, std::size_t line, std::size_t column);
    Token number(std::size_t start, std::size_t line, std::size_t column);
    Token character(std::size_t start, std::size_t line, std::size_t column);

    std::string source_;
    std::size_t current_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

} // namespace dune
