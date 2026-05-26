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

    passed = expect_tokens("fn add(a: int, b: int) -> int { return a + b; }\nlet ok: bool = true;",
                           {
                               {fn_keyword, "fn"},
                               {identifier, "add"},
                               {left_paren, "("},
                               {identifier, "a"},
                               {colon, ":"},
                               {int_keyword, "int"},
                               {comma, ","},
                               {identifier, "b"},
                               {colon, ":"},
                               {int_keyword, "int"},
                               {right_paren, ")"},
                               {arrow, "->"},
                               {int_keyword, "int"},
                               {left_brace, "{"},
                               {return_keyword, "return"},
                               {identifier, "a"},
                               {plus, "+"},
                               {identifier, "b"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {let, "let"},
                               {identifier, "ok"},
                               {colon, ":"},
                               {bool_keyword, "bool"},
                               {equal, "="},
                               {true_keyword, "true"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("let a: u8 = 1; let b: uint64 = 2; let c: real = 1.5; let d: glyph = 'x';",
                           {
                               {let, "let"},
                               {identifier, "a"},
                               {colon, ":"},
                               {u8_keyword, "u8"},
                               {equal, "="},
                               {number, "1"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "b"},
                               {colon, ":"},
                               {uint64_keyword, "uint64"},
                               {equal, "="},
                               {number, "2"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "c"},
                               {colon, ":"},
                               {real_keyword, "real"},
                               {equal, "="},
                               {float_number, "1.5"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "d"},
                               {colon, ":"},
                               {glyph_keyword, "glyph"},
                               {equal, "="},
                               {char_literal, "'x'"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("fn log(message: text) -> unit { return; } "
                           "let a: i8 = 1; let b: i16 = 2; let c: i32 = 3; let d: i64 = 4; "
                           "let e: isize = 5; let f: usize = 6; let g: real32 = 1.5; "
                           "let h: real64 = 2.5; log(\"ok\");",
                           {
                               {fn_keyword, "fn"},
                               {identifier, "log"},
                               {left_paren, "("},
                               {identifier, "message"},
                               {colon, ":"},
                               {text_keyword, "text"},
                               {right_paren, ")"},
                               {arrow, "->"},
                               {unit_keyword, "unit"},
                               {left_brace, "{"},
                               {return_keyword, "return"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {let, "let"},
                               {identifier, "a"},
                               {colon, ":"},
                               {i8_keyword, "i8"},
                               {equal, "="},
                               {number, "1"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "b"},
                               {colon, ":"},
                               {i16_keyword, "i16"},
                               {equal, "="},
                               {number, "2"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "c"},
                               {colon, ":"},
                               {i32_keyword, "i32"},
                               {equal, "="},
                               {number, "3"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "d"},
                               {colon, ":"},
                               {i64_keyword, "i64"},
                               {equal, "="},
                               {number, "4"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "e"},
                               {colon, ":"},
                               {isize_keyword, "isize"},
                               {equal, "="},
                               {number, "5"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "f"},
                               {colon, ":"},
                               {usize_keyword, "usize"},
                               {equal, "="},
                               {number, "6"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "g"},
                               {colon, ":"},
                               {real32_keyword, "real32"},
                               {equal, "="},
                               {float_number, "1.5"},
                               {semicolon, ";"},
                               {let, "let"},
                               {identifier, "h"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {equal, "="},
                               {float_number, "2.5"},
                               {semicolon, ";"},
                               {identifier, "log"},
                               {left_paren, "("},
                               {string_literal, "\"ok\""},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    return passed ? 0 : 1;
}
