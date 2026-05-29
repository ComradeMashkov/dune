#include "lexer/lexer.hpp"

#include <iostream>
#include <stdexcept>
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

bool expect_lex_error_contains(const std::string& source, const std::string& expected, const char* message) {
    try {
        dune::Lexer lexer(source);
        lexer.tokenize();
    } catch (const std::runtime_error& error) {
        const std::string actual = error.what();
        if (actual.find(expected) == std::string::npos) {
            std::cerr << message << ": expected error containing '" << expected << "', got '" << actual << "'\n";
            return false;
        }

        return true;
    }

    std::cerr << message << ": expected lexer error\n";
    return false;
}

} // namespace

int main() {
    using enum dune::TokenType;

    bool passed = true;

    passed = expect_tokens("x = 40 + 2;\nprint(x);",
                           {
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

    passed = expect_tokens("for value in values { print(value); } for i in 0..items.len() { print(i); }",
                           {
                               {for_keyword, "for"},
                               {identifier, "value"},
                               {in_keyword, "in"},
                               {identifier, "values"},
                               {left_brace, "{"},
                               {print, "print"},
                               {left_paren, "("},
                               {identifier, "value"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {for_keyword, "for"},
                               {identifier, "i"},
                               {in_keyword, "in"},
                               {number, "0"},
                               {dot_dot, ".."},
                               {identifier, "items"},
                               {dot, "."},
                               {identifier, "len"},
                               {left_paren, "("},
                               {right_paren, ")"},
                               {left_brace, "{"},
                               {print, "print"},
                               {left_paren, "("},
                               {identifier, "i"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {right_brace, "}"},
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

    passed = expect_tokens("fn add(a: int, b: int): int { return a + b; }\ndone: bool = true;",
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
                               {colon, ":"},
                               {int_keyword, "int"},
                               {left_brace, "{"},
                               {return_keyword, "return"},
                               {identifier, "a"},
                               {plus, "+"},
                               {identifier, "b"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {identifier, "done"},
                               {colon, ":"},
                               {bool_keyword, "bool"},
                               {equal, "="},
                               {true_keyword, "true"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("a: u8 = 1; b: uint64 = 2; c: real = 1.5; d: glyph = 'x';",
                           {
                               {identifier, "a"}, {colon, ":"},          {u8_keyword, "u8"},
                               {equal, "="},      {number, "1"},         {semicolon, ";"},
                               {identifier, "b"}, {colon, ":"},          {uint64_keyword, "uint64"},
                               {equal, "="},      {number, "2"},         {semicolon, ";"},
                               {identifier, "c"}, {colon, ":"},          {real_keyword, "real"},
                               {equal, "="},      {float_number, "1.5"}, {semicolon, ";"},
                               {identifier, "d"}, {colon, ":"},          {glyph_keyword, "glyph"},
                               {equal, "="},      {char_literal, "'x'"}, {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("size = 1_000_000; mask = 0xffu64; bits = 0b1010_0101u8; wide = 123i64;",
                           {
                               {identifier, "size"},
                               {equal, "="},
                               {number, "1_000_000"},
                               {semicolon, ";"},
                               {identifier, "mask"},
                               {equal, "="},
                               {number, "0xffu64"},
                               {semicolon, ";"},
                               {identifier, "bits"},
                               {equal, "="},
                               {number, "0b1010_0101u8"},
                               {semicolon, ";"},
                               {identifier, "wide"},
                               {equal, "="},
                               {number, "123i64"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("rough: real = 1_000.5_25;",
                           {
                               {identifier, "rough"},
                               {colon, ":"},
                               {real_keyword, "real"},
                               {equal, "="},
                               {float_number, "1_000.5_25"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_lex_error_contains("bad = 1__000;", "invalid numeric separator",
                                       "expected repeated numeric separator error") &&
             passed;
    passed = expect_lex_error_contains("bad = 0b102;", "invalid digit in binary literal",
                                       "expected invalid binary digit error") &&
             passed;
    passed = expect_lex_error_contains("bad = 42abc;", "invalid numeric literal suffix 'abc'",
                                       "expected invalid numeric suffix error") &&
             passed;

    passed = expect_tokens("fn log(message: text): unit { return; } "
                           "a: i8 = 1; b: i16 = 2; c: i32 = 3; d: i64 = 4; "
                           "e: isize = 5; f: usize = 6; g: real32 = 1.5; "
                           "h: real64 = 2.5; log(\"done\");",
                           {
                               {fn_keyword, "fn"},
                               {identifier, "log"},
                               {left_paren, "("},
                               {identifier, "message"},
                               {colon, ":"},
                               {text_keyword, "text"},
                               {right_paren, ")"},
                               {colon, ":"},
                               {unit_keyword, "unit"},
                               {left_brace, "{"},
                               {return_keyword, "return"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {identifier, "a"},
                               {colon, ":"},
                               {i8_keyword, "i8"},
                               {equal, "="},
                               {number, "1"},
                               {semicolon, ";"},
                               {identifier, "b"},
                               {colon, ":"},
                               {i16_keyword, "i16"},
                               {equal, "="},
                               {number, "2"},
                               {semicolon, ";"},
                               {identifier, "c"},
                               {colon, ":"},
                               {i32_keyword, "i32"},
                               {equal, "="},
                               {number, "3"},
                               {semicolon, ";"},
                               {identifier, "d"},
                               {colon, ":"},
                               {i64_keyword, "i64"},
                               {equal, "="},
                               {number, "4"},
                               {semicolon, ";"},
                               {identifier, "e"},
                               {colon, ":"},
                               {isize_keyword, "isize"},
                               {equal, "="},
                               {number, "5"},
                               {semicolon, ";"},
                               {identifier, "f"},
                               {colon, ":"},
                               {usize_keyword, "usize"},
                               {equal, "="},
                               {number, "6"},
                               {semicolon, ";"},
                               {identifier, "g"},
                               {colon, ":"},
                               {real32_keyword, "real32"},
                               {equal, "="},
                               {float_number, "1.5"},
                               {semicolon, ";"},
                               {identifier, "h"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {equal, "="},
                               {float_number, "2.5"},
                               {semicolon, ";"},
                               {identifier, "log"},
                               {left_paren, "("},
                               {string_literal, "\"done\""},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed =
        expect_tokens(
            R"dune(path: text = r"C:\Users\name\data.csv"; line: text = "hello\n"; tab: glyph = '\t'; quote: glyph = '\''; slash: glyph = '\\';)dune",
            {
                {identifier, "path"},
                {colon, ":"},
                {text_keyword, "text"},
                {equal, "="},
                {string_literal, R"(r"C:\Users\name\data.csv")"},
                {semicolon, ";"},
                {identifier, "line"},
                {colon, ":"},
                {text_keyword, "text"},
                {equal, "="},
                {string_literal, R"("hello\n")"},
                {semicolon, ";"},
                {identifier, "tab"},
                {colon, ":"},
                {glyph_keyword, "glyph"},
                {equal, "="},
                {char_literal, R"('\t')"},
                {semicolon, ";"},
                {identifier, "quote"},
                {colon, ":"},
                {glyph_keyword, "glyph"},
                {equal, "="},
                {char_literal, R"('\'')"},
                {semicolon, ";"},
                {identifier, "slash"},
                {colon, ":"},
                {glyph_keyword, "glyph"},
                {equal, "="},
                {char_literal, R"('\\')"},
                {semicolon, ";"},
                {eof, ""},
            }) &&
        passed;

    passed = expect_lex_error_contains(R"(bad: text = "\x";)", R"(unknown text escape '\x')",
                                       "expected invalid text escape error") &&
             passed;
    passed = expect_lex_error_contains(R"(bad: glyph = '\x';)", R"(unknown glyph escape '\x')",
                                       "expected invalid glyph escape error") &&
             passed;
    passed = expect_lex_error_contains("bad: glyph = '';", "invalid glyph literal",
                                       "expected invalid glyph literal error") &&
             passed;
    passed = expect_lex_error_contains("bad: text = r\"line\nnext\";", "unterminated raw string literal",
                                       "expected unterminated raw string error") &&
             passed;

    passed = expect_tokens("import math; values: [int] = [1, 2]; values.push(math.square(values[0])); "
                           "print(values.len());",
                           {
                               {import_keyword, "import"},
                               {identifier, "math"},
                               {semicolon, ";"},
                               {identifier, "values"},
                               {colon, ":"},
                               {left_bracket, "["},
                               {int_keyword, "int"},
                               {right_bracket, "]"},
                               {equal, "="},
                               {left_bracket, "["},
                               {number, "1"},
                               {comma, ","},
                               {number, "2"},
                               {right_bracket, "]"},
                               {semicolon, ";"},
                               {identifier, "values"},
                               {dot, "."},
                               {identifier, "push"},
                               {left_paren, "("},
                               {identifier, "math"},
                               {dot, "."},
                               {identifier, "square"},
                               {left_paren, "("},
                               {identifier, "values"},
                               {left_bracket, "["},
                               {number, "0"},
                               {right_bracket, "]"},
                               {right_paren, ")"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {print, "print"},
                               {left_paren, "("},
                               {identifier, "values"},
                               {dot, "."},
                               {identifier, "len"},
                               {left_paren, "("},
                               {right_paren, ")"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("const tau: real64 = math.PI * 2.0;",
                           {
                               {const_keyword, "const"},
                               {identifier, "tau"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {equal, "="},
                               {identifier, "math"},
                               {dot, "."},
                               {identifier, "PI"},
                               {star, "*"},
                               {float_number, "2.0"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("done: bool = !false && true || (17 % 5 == 2); x: real64 = 17 to real64;",
                           {
                               {identifier, "done"},
                               {colon, ":"},
                               {bool_keyword, "bool"},
                               {equal, "="},
                               {bang, "!"},
                               {false_keyword, "false"},
                               {amp_amp, "&&"},
                               {true_keyword, "true"},
                               {pipe_pipe, "||"},
                               {left_paren, "("},
                               {number, "17"},
                               {percent, "%"},
                               {number, "5"},
                               {equal_equal, "=="},
                               {number, "2"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {identifier, "x"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {equal, "="},
                               {number, "17"},
                               {to_keyword, "to"},
                               {real64_keyword, "real64"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("export foreign fn c_sqrt(value: real64): real64 = \"sqrt\"; "
                           "for i = 0; i < 3; i = i + 1 { if i == 1 { continue; } break; } "
                           "print(\"dune\"[1:3]);",
                           {
                               {export_keyword, "export"},
                               {foreign_keyword, "foreign"},
                               {fn_keyword, "fn"},
                               {identifier, "c_sqrt"},
                               {left_paren, "("},
                               {identifier, "value"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {right_paren, ")"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {equal, "="},
                               {string_literal, "\"sqrt\""},
                               {semicolon, ";"},
                               {for_keyword, "for"},
                               {identifier, "i"},
                               {equal, "="},
                               {number, "0"},
                               {semicolon, ";"},
                               {identifier, "i"},
                               {less, "<"},
                               {number, "3"},
                               {semicolon, ";"},
                               {identifier, "i"},
                               {equal, "="},
                               {identifier, "i"},
                               {plus, "+"},
                               {number, "1"},
                               {left_brace, "{"},
                               {if_keyword, "if"},
                               {identifier, "i"},
                               {equal_equal, "=="},
                               {number, "1"},
                               {left_brace, "{"},
                               {continue_keyword, "continue"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {break_keyword, "break"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {print, "print"},
                               {left_paren, "("},
                               {string_literal, "\"dune\""},
                               {left_bracket, "["},
                               {number, "1"},
                               {colon, ":"},
                               {number, "3"},
                               {right_bracket, "]"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("// leading comment\n"
                           "export method<T> [T].first(): T { // methods\n"
                           "return this[0];\n"
                           "} // trailing comment\n"
                           "half = 8 / 2;",
                           {
                               {export_keyword, "export"},
                               {method_keyword, "method"},
                               {less, "<"},
                               {identifier, "T"},
                               {greater, ">"},
                               {left_bracket, "["},
                               {identifier, "T"},
                               {right_bracket, "]"},
                               {dot, "."},
                               {identifier, "first"},
                               {left_paren, "("},
                               {right_paren, ")"},
                               {colon, ":"},
                               {identifier, "T"},
                               {left_brace, "{"},
                               {return_keyword, "return"},
                               {identifier, "this"},
                               {left_bracket, "["},
                               {number, "0"},
                               {right_bracket, "]"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {identifier, "half"},
                               {equal, "="},
                               {number, "8"},
                               {slash, "/"},
                               {number, "2"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("export record Point { x: real64, y: real64 } "
                           "p: Point = Point { x: 1.0, y: 2.0 }; print(p.x);",
                           {
                               {export_keyword, "export"},
                               {record_keyword, "record"},
                               {identifier, "Point"},
                               {left_brace, "{"},
                               {identifier, "x"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {comma, ","},
                               {identifier, "y"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {right_brace, "}"},
                               {identifier, "p"},
                               {colon, ":"},
                               {identifier, "Point"},
                               {equal, "="},
                               {identifier, "Point"},
                               {left_brace, "{"},
                               {identifier, "x"},
                               {colon, ":"},
                               {float_number, "1.0"},
                               {comma, ","},
                               {identifier, "y"},
                               {colon, ":"},
                               {float_number, "2.0"},
                               {right_brace, "}"},
                               {semicolon, ";"},
                               {print, "print"},
                               {left_paren, "("},
                               {identifier, "p"},
                               {dot, "."},
                               {identifier, "x"},
                               {right_paren, ")"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed =
        expect_tokens("export contract Shape { area(): real64; } record Circle with Shape { export radius: real64 }",
                      {
                          {export_keyword, "export"},
                          {contract_keyword, "contract"},
                          {identifier, "Shape"},
                          {left_brace, "{"},
                          {identifier, "area"},
                          {left_paren, "("},
                          {right_paren, ")"},
                          {colon, ":"},
                          {real64_keyword, "real64"},
                          {semicolon, ";"},
                          {right_brace, "}"},
                          {record_keyword, "record"},
                          {identifier, "Circle"},
                          {with_keyword, "with"},
                          {identifier, "Shape"},
                          {left_brace, "{"},
                          {export_keyword, "export"},
                          {identifier, "radius"},
                          {colon, ":"},
                          {real64_keyword, "real64"},
                          {right_brace, "}"},
                          {eof, ""},
                      }) &&
        passed;

    passed = expect_tokens("out = when value { is 1 { 10 } is _ { 20 } };",
                           {
                               {identifier, "out"},
                               {equal, "="},
                               {when_keyword, "when"},
                               {identifier, "value"},
                               {left_brace, "{"},
                               {is_keyword, "is"},
                               {number, "1"},
                               {left_brace, "{"},
                               {number, "10"},
                               {right_brace, "}"},
                               {is_keyword, "is"},
                               {identifier, "_"},
                               {left_brace, "{"},
                               {number, "20"},
                               {right_brace, "}"},
                               {right_brace, "}"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("export choice Maybe<T> { Present(T), Absent, }",
                           {
                               {export_keyword, "export"},
                               {choice_keyword, "choice"},
                               {identifier, "Maybe"},
                               {less, "<"},
                               {identifier, "T"},
                               {greater, ">"},
                               {left_brace, "{"},
                               {identifier, "Present"},
                               {left_paren, "("},
                               {identifier, "T"},
                               {right_paren, ")"},
                               {comma, ","},
                               {identifier, "Absent"},
                               {comma, ","},
                               {right_brace, "}"},
                               {eof, ""},
                           }) &&
             passed;

    return passed ? 0 : 1;
}
