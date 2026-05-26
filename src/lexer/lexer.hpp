#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace dune {

enum class TokenType {
    let,
    const_keyword,
    export_keyword,
    extern_keyword,
    fn_keyword,
    impl_keyword,
    struct_keyword,
    import_keyword,
    return_keyword,
    print,
    if_keyword,
    else_keyword,
    while_keyword,
    for_keyword,
    break_keyword,
    continue_keyword,
    as_keyword,
    int_keyword,
    bool_keyword,
    i8_keyword,
    i16_keyword,
    i32_keyword,
    i64_keyword,
    isize_keyword,
    u8_keyword,
    u16_keyword,
    u32_keyword,
    u64_keyword,
    usize_keyword,
    uint8_keyword,
    uint16_keyword,
    uint32_keyword,
    uint64_keyword,
    real32_keyword,
    real64_keyword,
    real_keyword,
    glyph_keyword,
    text_keyword,
    unit_keyword,
    true_keyword,
    false_keyword,
    identifier,
    number,
    float_number,
    char_literal,
    string_literal,
    plus,
    minus,
    arrow,
    star,
    slash,
    percent,
    bang,
    equal,
    equal_equal,
    bang_equal,
    amp_amp,
    pipe_pipe,
    greater,
    greater_equal,
    less,
    less_equal,
    colon,
    comma,
    dot,
    semicolon,
    left_paren,
    right_paren,
    left_brace,
    right_brace,
    left_bracket,
    right_bracket,
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
    Token string(std::size_t start, std::size_t line, std::size_t column);

    std::string source_;
    std::size_t current_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

} // namespace dune
