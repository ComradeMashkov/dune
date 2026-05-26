#include "lexer.hpp"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace dune {

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

Token Lexer::next_token() {
    skip_whitespace();

    if (is_at_end()) {
        return Token{TokenType::eof, ""};
    }

    const std::size_t start = current_;
    const char current = advance();

    if (std::isalpha(static_cast<unsigned char>(current)) || current == '_') {
        return identifier(start);
    }

    if (std::isdigit(static_cast<unsigned char>(current))) {
        return number(start);
    }

    switch (current) {
    case '+':
        return make_token(TokenType::plus, start);
    case '-':
        return make_token(TokenType::minus, start);
    case '*':
        return make_token(TokenType::star, start);
    case '/':
        return make_token(TokenType::slash, start);
    case '=':
        if (match('=')) {
            return make_token(TokenType::equal_equal, start);
        }

        return make_token(TokenType::equal, start);
    case '!':
        if (match('=')) {
            return make_token(TokenType::bang_equal, start);
        }

        break;
    case '>':
        if (match('=')) {
            return make_token(TokenType::greater_equal, start);
        }

        return make_token(TokenType::greater, start);
    case '<':
        if (match('=')) {
            return make_token(TokenType::less_equal, start);
        }

        return make_token(TokenType::less, start);
    case ';':
        return make_token(TokenType::semicolon, start);
    case '(':
        return make_token(TokenType::left_paren, start);
    case ')':
        return make_token(TokenType::right_paren, start);
    case '{':
        return make_token(TokenType::left_brace, start);
    case '}':
        return make_token(TokenType::right_brace, start);
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
    return source_[current_++];
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[current_] != expected) {
        return false;
    }

    ++current_;
    return true;
}

char Lexer::peek() const {
    if (is_at_end()) {
        return '\0';
    }

    return source_[current_];
}

void Lexer::skip_whitespace() {
    while (!is_at_end() && std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

Token Lexer::make_token(TokenType type, std::size_t start) const {
    return Token{type, source_.substr(start, current_ - start)};
}

Token Lexer::identifier(std::size_t start) {
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        advance();
    }

    const std::string lexeme = source_.substr(start, current_ - start);
    if (lexeme == "let") {
        return Token{TokenType::let, lexeme};
    }

    if (lexeme == "print") {
        return Token{TokenType::print, lexeme};
    }

    if (lexeme == "if") {
        return Token{TokenType::if_keyword, lexeme};
    }

    if (lexeme == "else") {
        return Token{TokenType::else_keyword, lexeme};
    }

    if (lexeme == "while") {
        return Token{TokenType::while_keyword, lexeme};
    }

    if (lexeme == "true") {
        return Token{TokenType::true_keyword, lexeme};
    }

    if (lexeme == "false") {
        return Token{TokenType::false_keyword, lexeme};
    }

    return Token{TokenType::identifier, lexeme};
}

Token Lexer::number(std::size_t start) {
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    return make_token(TokenType::number, start);
}

} // namespace dune
