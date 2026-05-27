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

    passed = expect_tokens("var x = 40 + 2;\nprint(x);",
                           {
                               {var_keyword, "var"},
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

    passed = expect_tokens("func add(a: int, b: int): int { return a + b; }\nvar done: bool = true;",
                           {
                               {func_keyword, "func"},
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
                               {var_keyword, "var"},
                               {identifier, "done"},
                               {colon, ":"},
                               {bool_keyword, "bool"},
                               {equal, "="},
                               {true_keyword, "true"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("var a: u8 = 1; var b: uint64 = 2; var c: real = 1.5; var d: glyph = 'x';",
                           {
                               {var_keyword, "var"},
                               {identifier, "a"},
                               {colon, ":"},
                               {u8_keyword, "u8"},
                               {equal, "="},
                               {number, "1"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "b"},
                               {colon, ":"},
                               {uint64_keyword, "uint64"},
                               {equal, "="},
                               {number, "2"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "c"},
                               {colon, ":"},
                               {real_keyword, "real"},
                               {equal, "="},
                               {float_number, "1.5"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "d"},
                               {colon, ":"},
                               {glyph_keyword, "glyph"},
                               {equal, "="},
                               {char_literal, "'x'"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("func log(message: text): unit { return; } "
                           "var a: i8 = 1; var b: i16 = 2; var c: i32 = 3; var d: i64 = 4; "
                           "var e: isize = 5; var f: usize = 6; var g: real32 = 1.5; "
                           "var h: real64 = 2.5; log(\"done\");",
                           {
                               {func_keyword, "func"},
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
                               {var_keyword, "var"},
                               {identifier, "a"},
                               {colon, ":"},
                               {i8_keyword, "i8"},
                               {equal, "="},
                               {number, "1"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "b"},
                               {colon, ":"},
                               {i16_keyword, "i16"},
                               {equal, "="},
                               {number, "2"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "c"},
                               {colon, ":"},
                               {i32_keyword, "i32"},
                               {equal, "="},
                               {number, "3"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "d"},
                               {colon, ":"},
                               {i64_keyword, "i64"},
                               {equal, "="},
                               {number, "4"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "e"},
                               {colon, ":"},
                               {isize_keyword, "isize"},
                               {equal, "="},
                               {number, "5"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "f"},
                               {colon, ":"},
                               {usize_keyword, "usize"},
                               {equal, "="},
                               {number, "6"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
                               {identifier, "g"},
                               {colon, ":"},
                               {real32_keyword, "real32"},
                               {equal, "="},
                               {float_number, "1.5"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
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

    passed = expect_tokens("import math; var values: [int] = [1, 2]; values.push(math.square(values[0])); "
                           "print(values.len());",
                           {
                               {import_keyword, "import"},
                               {identifier, "math"},
                               {semicolon, ";"},
                               {var_keyword, "var"},
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

    passed = expect_tokens("var done: bool = !false && true || (17 % 5 == 2); var x: real64 = 17 to real64;",
                           {
                               {var_keyword, "var"},
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
                               {var_keyword, "var"},
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

    passed = expect_tokens("export foreign func c_sqrt(value: real64): real64 = \"sqrt\"; "
                           "for var i = 0; i < 3; i = i + 1 { if i == 1 { continue; } break; } "
                           "print(\"dune\"[1:3]);",
                           {
                               {export_keyword, "export"},
                               {foreign_keyword, "foreign"},
                               {func_keyword, "func"},
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
                               {var_keyword, "var"},
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
                           "export extend<T> [T] { // methods\n"
                           "func first(): T { return this[0]; }\n"
                           "} // trailing comment\n"
                           "var half = 8 / 2;",
                           {
                               {export_keyword, "export"},
                               {extend_keyword, "extend"},
                               {less, "<"},
                               {identifier, "T"},
                               {greater, ">"},
                               {left_bracket, "["},
                               {identifier, "T"},
                               {right_bracket, "]"},
                               {left_brace, "{"},
                               {func_keyword, "func"},
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
                               {right_brace, "}"},
                               {var_keyword, "var"},
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
                           "var p: Point = Point { x: 1.0, y: 2.0 }; print(p.x);",
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
                               {var_keyword, "var"},
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

    passed = expect_tokens("var out = case value { 1 : 10, _ : 20 };",
                           {
                               {var_keyword, "var"},
                               {identifier, "out"},
                               {equal, "="},
                               {case_keyword, "case"},
                               {identifier, "value"},
                               {left_brace, "{"},
                               {number, "1"},
                               {colon, ":"},
                               {number, "10"},
                               {comma, ","},
                               {identifier, "_"},
                               {colon, ":"},
                               {number, "20"},
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
