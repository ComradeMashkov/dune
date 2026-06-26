#include "lexer.hpp"

#include <cctype>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace dune {

namespace {

std::string escape_name(char value) {
    switch (value) {
    case '\0':
        return "\\0";
    case '\n':
        return "\\n";
    case '\r':
        return "\\r";
    case '\t':
        return "\\t";
    default:
        return "\\" + std::string(1, value);
    }
}

bool is_text_escape(char value) {
    switch (value) {
    case 'n':
    case 'r':
    case 't':
    case '0':
    case '"':
    case '\\':
        return true;
    default:
        return false;
    }
}

bool is_glyph_escape(char value) {
    switch (value) {
    case 'n':
    case 'r':
    case 't':
    case '0':
    case '\'':
    case '\\':
        return true;
    default:
        return false;
    }
}

bool is_hex_digit(char value) {
    return std::isdigit(static_cast<unsigned char>(value)) || (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

bool is_binary_digit(char value) {
    return value == '0' || value == '1';
}

bool is_integer_suffix(std::string_view suffix) {
    return suffix == "i8" || suffix == "i16" || suffix == "i32" || suffix == "i64" || suffix == "isize" ||
           suffix == "u8" || suffix == "u16" || suffix == "u32" || suffix == "u64" || suffix == "usize";
}

} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

Token Lexer::next_token() {
    skip_whitespace();

    if (is_at_end()) {
        return Token{TokenType::eof, "", line_, column_};
    }

    const std::size_t start = current_;
    const std::size_t line = line_;
    const std::size_t column = column_;
    const char current = advance();

    if (current == 'r' && peek() == '"') {
        advance();
        return raw_string(start, line, column);
    }

    if (std::isalpha(static_cast<unsigned char>(current)) || current == '_') {
        return identifier(start, line, column);
    }

    if (std::isdigit(static_cast<unsigned char>(current))) {
        return number(start, line, column);
    }

    switch (current) {
    case '"':
        return string(start, line, column);
    case '\'':
        return character(start, line, column);
    case '+':
        return make_token(TokenType::plus, start, line, column);
    case '-':
        if (match('>')) {
            return make_token(TokenType::arrow, start, line, column);
        }

        return make_token(TokenType::minus, start, line, column);
    case '*':
        return make_token(TokenType::star, start, line, column);
    case '/':
        return make_token(TokenType::slash, start, line, column);
    case '%':
        return make_token(TokenType::percent, start, line, column);
    case '=':
        if (match('>')) {
            return make_token(TokenType::fat_arrow, start, line, column);
        }

        if (match('=')) {
            return make_token(TokenType::equal_equal, start, line, column);
        }

        return make_token(TokenType::equal, start, line, column);
    case '!':
        if (match('=')) {
            return make_token(TokenType::bang_equal, start, line, column);
        }

        return make_token(TokenType::bang, start, line, column);
    case '&':
        if (match('&')) {
            return make_token(TokenType::amp_amp, start, line, column);
        }

        break;
    case '|':
        if (match('|')) {
            return make_token(TokenType::pipe_pipe, start, line, column);
        }

        break;
    case '>':
        if (match('=')) {
            return make_token(TokenType::greater_equal, start, line, column);
        }

        return make_token(TokenType::greater, start, line, column);
    case '<':
        if (match('=')) {
            return make_token(TokenType::less_equal, start, line, column);
        }

        return make_token(TokenType::less, start, line, column);
    case ':':
        return make_token(TokenType::colon, start, line, column);
    case ',':
        return make_token(TokenType::comma, start, line, column);
    case '.':
        if (match('.')) {
            return make_token(TokenType::dot_dot, start, line, column);
        }

        return make_token(TokenType::dot, start, line, column);
    case '?':
        return make_token(TokenType::question, start, line, column);
    case ';':
        return make_token(TokenType::semicolon, start, line, column);
    case '(':
        return make_token(TokenType::left_paren, start, line, column);
    case ')':
        return make_token(TokenType::right_paren, start, line, column);
    case '{':
        return make_token(TokenType::left_brace, start, line, column);
    case '}':
        return make_token(TokenType::right_brace, start, line, column);
    case '[':
        return make_token(TokenType::left_bracket, start, line, column);
    case ']':
        return make_token(TokenType::right_bracket, start, line, column);
    default:
        break;
    }

    throw std::runtime_error("unexpected character in source");
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        Token token = next_token();
        tokens.push_back(token);

        if (token.type == TokenType::eof) {
            break;
        }
    }

    return tokens;
}

bool Lexer::is_at_end() const {
    return current_ >= source_.size();
}

char Lexer::advance() {
    const char current = source_[current_++];
    if (current == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }

    return current;
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[current_] != expected) {
        return false;
    }

    advance();
    return true;
}

char Lexer::peek() const {
    if (is_at_end()) {
        return '\0';
    }

    return source_[current_];
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        if (std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
            continue;
        }

        if (peek() == '/' && current_ + 1 < source_.size() && source_[current_ + 1] == '/') {
            while (!is_at_end() && peek() != '\n') {
                advance();
            }
            continue;
        }

        break;
    }
}

Token Lexer::make_token(TokenType type, std::size_t start, std::size_t line, std::size_t column) const {
    return Token{type, source_.substr(start, current_ - start), line, column};
}

Token Lexer::identifier(std::size_t start, std::size_t line, std::size_t column) {
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        advance();
    }

    const std::string lexeme = source_.substr(start, current_ - start);
    if (lexeme == "const") {
        return Token{TokenType::const_keyword, lexeme, line, column};
    }

    if (lexeme == "export") {
        return Token{TokenType::export_keyword, lexeme, line, column};
    }

    if (lexeme == "foreign") {
        return Token{TokenType::foreign_keyword, lexeme, line, column};
    }

    if (lexeme == "fn") {
        return Token{TokenType::fn_keyword, lexeme, line, column};
    }

    if (lexeme == "method") {
        return Token{TokenType::method_keyword, lexeme, line, column};
    }

    if (lexeme == "record") {
        return Token{TokenType::record_keyword, lexeme, line, column};
    }

    if (lexeme == "contract") {
        return Token{TokenType::contract_keyword, lexeme, line, column};
    }

    if (lexeme == "with") {
        return Token{TokenType::with_keyword, lexeme, line, column};
    }

    if (lexeme == "choice") {
        return Token{TokenType::choice_keyword, lexeme, line, column};
    }

    if (lexeme == "import") {
        return Token{TokenType::import_keyword, lexeme, line, column};
    }

    if (lexeme == "when") {
        return Token{TokenType::when_keyword, lexeme, line, column};
    }

    if (lexeme == "return") {
        return Token{TokenType::return_keyword, lexeme, line, column};
    }

    if (lexeme == "print") {
        return Token{TokenType::print, lexeme, line, column};
    }

    if (lexeme == "if") {
        return Token{TokenType::if_keyword, lexeme, line, column};
    }

    if (lexeme == "else") {
        return Token{TokenType::else_keyword, lexeme, line, column};
    }

    if (lexeme == "while") {
        return Token{TokenType::while_keyword, lexeme, line, column};
    }

    if (lexeme == "for") {
        return Token{TokenType::for_keyword, lexeme, line, column};
    }

    if (lexeme == "in") {
        return Token{TokenType::in_keyword, lexeme, line, column};
    }

    if (lexeme == "break") {
        return Token{TokenType::break_keyword, lexeme, line, column};
    }

    if (lexeme == "continue") {
        return Token{TokenType::continue_keyword, lexeme, line, column};
    }

    if (lexeme == "static") {
        return Token{TokenType::static_keyword, lexeme, line, column};
    }

    if (lexeme == "to") {
        return Token{TokenType::to_keyword, lexeme, line, column};
    }

    if (lexeme == "type") {
        return Token{TokenType::type_keyword, lexeme, line, column};
    }

    if (lexeme == "is") {
        return Token{TokenType::is_keyword, lexeme, line, column};
    }

    if (lexeme == "int") {
        return Token{TokenType::int_keyword, lexeme, line, column};
    }

    if (lexeme == "bool") {
        return Token{TokenType::bool_keyword, lexeme, line, column};
    }

    if (lexeme == "i8") {
        return Token{TokenType::i8_keyword, lexeme, line, column};
    }

    if (lexeme == "i16") {
        return Token{TokenType::i16_keyword, lexeme, line, column};
    }

    if (lexeme == "i32") {
        return Token{TokenType::i32_keyword, lexeme, line, column};
    }

    if (lexeme == "i64") {
        return Token{TokenType::i64_keyword, lexeme, line, column};
    }

    if (lexeme == "isize") {
        return Token{TokenType::isize_keyword, lexeme, line, column};
    }

    if (lexeme == "u8") {
        return Token{TokenType::u8_keyword, lexeme, line, column};
    }

    if (lexeme == "u16") {
        return Token{TokenType::u16_keyword, lexeme, line, column};
    }

    if (lexeme == "u32") {
        return Token{TokenType::u32_keyword, lexeme, line, column};
    }

    if (lexeme == "u64") {
        return Token{TokenType::u64_keyword, lexeme, line, column};
    }

    if (lexeme == "usize") {
        return Token{TokenType::usize_keyword, lexeme, line, column};
    }

    if (lexeme == "uint8") {
        return Token{TokenType::uint8_keyword, lexeme, line, column};
    }

    if (lexeme == "uint16") {
        return Token{TokenType::uint16_keyword, lexeme, line, column};
    }

    if (lexeme == "uint32") {
        return Token{TokenType::uint32_keyword, lexeme, line, column};
    }

    if (lexeme == "uint64") {
        return Token{TokenType::uint64_keyword, lexeme, line, column};
    }

    if (lexeme == "real32") {
        return Token{TokenType::real32_keyword, lexeme, line, column};
    }

    if (lexeme == "real64") {
        return Token{TokenType::real64_keyword, lexeme, line, column};
    }

    if (lexeme == "real") {
        return Token{TokenType::real_keyword, lexeme, line, column};
    }

    if (lexeme == "glyph") {
        return Token{TokenType::glyph_keyword, lexeme, line, column};
    }

    if (lexeme == "text") {
        return Token{TokenType::text_keyword, lexeme, line, column};
    }

    if (lexeme == "unit") {
        return Token{TokenType::unit_keyword, lexeme, line, column};
    }

    if (lexeme == "true") {
        return Token{TokenType::true_keyword, lexeme, line, column};
    }

    if (lexeme == "false") {
        return Token{TokenType::false_keyword, lexeme, line, column};
    }

    return Token{TokenType::identifier, lexeme, line, column};
}

Token Lexer::number(std::size_t start, std::size_t line, std::size_t column) {
    auto consume_digits = [&](auto is_digit, std::string_view description, bool saw_initial_digit = false) {
        bool saw_digit = saw_initial_digit;
        bool previous_separator = false;
        while (is_digit(peek()) || peek() == '_') {
            if (peek() == '_') {
                if (!saw_digit || previous_separator) {
                    throw std::runtime_error("invalid numeric separator");
                }
                previous_separator = true;
                advance();
                continue;
            }

            saw_digit = true;
            previous_separator = false;
            advance();
        }

        if (!saw_digit) {
            throw std::runtime_error("expected digit in " + std::string(description) + " literal");
        }

        if (previous_separator) {
            throw std::runtime_error("invalid numeric separator");
        }
    };

    auto consume_suffix = [&]() {
        if (!std::isalpha(static_cast<unsigned char>(peek()))) {
            return;
        }

        const std::size_t suffix_start = current_;
        while (std::isalnum(static_cast<unsigned char>(peek()))) {
            advance();
        }

        const std::string suffix = source_.substr(suffix_start, current_ - suffix_start);
        if (!is_integer_suffix(suffix)) {
            throw std::runtime_error("invalid numeric literal suffix '" + suffix + "'");
        }
    };

    if (source_[start] == '0' && (peek() == 'x' || peek() == 'X')) {
        advance();
        consume_digits(is_hex_digit, "hex");
        consume_suffix();
        return make_token(TokenType::number, start, line, column);
    }

    if (source_[start] == '0' && (peek() == 'b' || peek() == 'B')) {
        advance();
        consume_digits(is_binary_digit, "binary");
        if (std::isdigit(static_cast<unsigned char>(peek()))) {
            throw std::runtime_error("invalid digit in binary literal");
        }
        consume_suffix();
        return make_token(TokenType::number, start, line, column);
    }

    consume_digits([](char value) { return std::isdigit(static_cast<unsigned char>(value)); }, "decimal", true);

    if (peek() == '.' && current_ + 1 < source_.size() &&
        std::isdigit(static_cast<unsigned char>(source_[current_ + 1]))) {
        advance();

        consume_digits([](char value) { return std::isdigit(static_cast<unsigned char>(value)); }, "decimal", true);
        if (std::isalpha(static_cast<unsigned char>(peek()))) {
            throw std::runtime_error("float literal suffixes are not supported");
        }

        return make_token(TokenType::float_number, start, line, column);
    }

    consume_suffix();
    return make_token(TokenType::number, start, line, column);
}

Token Lexer::character(std::size_t start, std::size_t line, std::size_t column) {
    if (is_at_end() || peek() == '\n') {
        throw std::runtime_error("unterminated character literal");
    }

    if (peek() == '\\') {
        advance();
        if (is_at_end() || peek() == '\n') {
            throw std::runtime_error("unterminated character literal");
        }

        if (!is_glyph_escape(peek())) {
            throw std::runtime_error("unknown glyph escape '" + escape_name(peek()) + "'");
        }
    } else if (peek() == '\'') {
        throw std::runtime_error("invalid glyph literal");
    }

    advance();

    if (!match('\'')) {
        throw std::runtime_error("expected closing quote after character literal");
    }

    return make_token(TokenType::char_literal, start, line, column);
}

Token Lexer::string(std::size_t start, std::size_t line, std::size_t column) {
    while (!is_at_end() && peek() != '"') {
        if (peek() == '\n') {
            throw std::runtime_error("unterminated string literal");
        }

        if (peek() == '\\') {
            advance();
            if (is_at_end() || peek() == '\n') {
                throw std::runtime_error("unterminated string literal");
            }

            if (!is_text_escape(peek())) {
                throw std::runtime_error("unknown text escape '" + escape_name(peek()) + "'");
            }
        }

        advance();
    }

    if (!match('"')) {
        throw std::runtime_error("unterminated string literal");
    }

    return make_token(TokenType::string_literal, start, line, column);
}

Token Lexer::raw_string(std::size_t start, std::size_t line, std::size_t column) {
    while (!is_at_end() && peek() != '"') {
        if (peek() == '\n') {
            throw std::runtime_error("unterminated raw string literal");
        }

        advance();
    }

    if (!match('"')) {
        throw std::runtime_error("unterminated raw string literal");
    }

    return make_token(TokenType::string_literal, start, line, column);
}

} // namespace dune
