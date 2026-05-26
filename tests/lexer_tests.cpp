#include "lexer/lexer.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

struct ExpectedToken {
    dune::TokenType type;
    std::string lexeme;
};

bool expect_tokens(const std::string& source, const std::vector<ExpectedToken>& expected) {
    dune::Lexer lexer(source);
    const std::vector<dune::Token> tokens = lexer.tokenize();

    if (tokens.size() != expected.size()) {
        std::cerr << "expected " << expected.size() << " tokens, got " << tokens.size() << '\n';
        return false;
    }

    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (tokens[index].type != expected[index].type || tokens[index].lexeme != expected[index].lexeme) {
            std::cerr << "token " << index << " did not match\n";
            return false;
        }
    }

    return true;
}

} // namespace

int main() {
    using enum dune::TokenType;

    bool passed = true;

    passed = expect_tokens("let x = 40 + 2;\nprint(x);",
                           {
                               {let, "let"},
                               {identifier, "x"},
                               {equal, "="},
                               {number, "40"},
                               {plus, "+"},
                               {number, "2"},
                               {semicolon, ";"},
                               {print, "print"},
                               {left_paren, "("},
                               {identifier, "x"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("foo - 7 * bar / 3",
                           {
                               {identifier, "foo"},
                               {minus, "-"},
                               {number, "7"},
                               {star, "*"},
                               {identifier, "bar"},
                               {slash, "/"},
                               {number, "3"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("if true { x = 3; } else { while x >= 0 { x = x - 1; } print(x == false); }",
                           {
                               {if_keyword, "if"},
                               {true_keyword, "true"},
                               {left_brace, "{"},
                               {identifier, "x"},
                               {equal, "="},
                               {number, "3"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {else_keyword, "else"},
                               {left_brace, "{"},
                               {while_keyword, "while"},
                               {identifier, "x"},
                               {greater_equal, ">="},
                               {number, "0"},
                               {left_brace, "{"},
                               {identifier, "x"},
                               {equal, "="},
                               {identifier, "x"},
                               {minus, "-"},
                               {number, "1"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {print, "print"},
                               {left_paren, "("},
                               {identifier, "x"},
                               {equal_equal, "=="},
                               {false_keyword, "false"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {eof, ""},
                           }) &&
             passed;

    return passed ? 0 : 1;
}
