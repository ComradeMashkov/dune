#include "lexer.hpp"

#include <cctype>
#include <stdexcept>
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
        return make_token(TokenType::dot, start, line, column);
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

    if (lexeme == "break") {
        return Token{TokenType::break_keyword, lexeme, line, column};
    }

    if (lexeme == "continue") {
        return Token{TokenType::continue_keyword, lexeme, line, column};
    }

    if (lexeme == "to") {
        return Token{TokenType::to_keyword, lexeme, line, column};
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
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    if (peek() == '.') {
        advance();
        if (!std::isdigit(static_cast<unsigned char>(peek()))) {
            throw std::runtime_error("expected digit after decimal point");
        }

        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }

        return make_token(TokenType::float_number, start, line, column);
    }

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
