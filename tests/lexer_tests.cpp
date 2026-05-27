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

    passed = expect_tokens("import math; let values: [int] = [1, 2]; values.push(math.square(values[0])); "
                           "print(values.len());",
                           {
                               {import_keyword, "import"},
                               {identifier, "math"},
                               {semicolon, ";"},
                               {let, "let"},
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

    passed = expect_tokens("let ok: bool = !false && true || (17 % 5 == 2); let x: real64 = 17 as real64;",
                           {
                               {let, "let"},
                               {identifier, "ok"},
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
                               {let, "let"},
                               {identifier, "x"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {equal, "="},
                               {number, "17"},
                               {as_keyword, "as"},
                               {real64_keyword, "real64"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("export extern fn c_sqrt(value: real64) -> real64 = \"sqrt\"; "
                           "for let i = 0; i < 3; i = i + 1 { if i == 1 { continue; } break; } "
                           "print(\"dune\"[1:3]);",
                           {
                               {export_keyword, "export"},
                               {extern_keyword, "extern"},
                               {fn_keyword, "fn"},
                               {identifier, "c_sqrt"},
                               {left_paren, "("},
                               {identifier, "value"},
                               {colon, ":"},
                               {real64_keyword, "real64"},
                               {right_paren, ")"},
                               {arrow, "->"},
                               {real64_keyword, "real64"},
                               {equal, "="},
                               {string_literal, "\"sqrt\""},
                               {semicolon, ";"},
                               {for_keyword, "for"},
                               {let, "let"},
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
                           "export impl<T> [T] { // methods\n"
                           "fn first() -> T { return self[0]; }\n"
                           "} // trailing comment\n"
                           "let half = 8 / 2;",
                           {
                               {export_keyword, "export"},
                               {impl_keyword, "impl"},
                               {less, "<"},
                               {identifier, "T"},
                               {greater, ">"},
                               {left_bracket, "["},
                               {identifier, "T"},
                               {right_bracket, "]"},
                               {left_brace, "{"},
                               {fn_keyword, "fn"},
                               {identifier, "first"},
                               {left_paren, "("},
                               {right_paren, ")"},
                               {arrow, "->"},
                               {identifier, "T"},
                               {left_brace, "{"},
                               {return_keyword, "return"},
                               {identifier, "self"},
                               {left_bracket, "["},
                               {number, "0"},
                               {right_bracket, "]"},
                               {semicolon, ";"},
                               {right_brace, "}"},
                               {right_brace, "}"},
                               {let, "let"},
                               {identifier, "half"},
                               {equal, "="},
                               {number, "8"},
                               {slash, "/"},
                               {number, "2"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("export struct Point { x: real64, y: real64 } "
                           "let p: Point = Point { x: 1.0, y: 2.0 }; print(p.x);",
                           {
                               {export_keyword, "export"},
                               {struct_keyword, "struct"},
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
                               {let, "let"},
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

    passed = expect_tokens("let out = match value { 1 => 10, _ => 20 };",
                           {
                               {let, "let"},
                               {identifier, "out"},
                               {equal, "="},
                               {match_keyword, "match"},
                               {identifier, "value"},
                               {left_brace, "{"},
                               {number, "1"},
                               {fat_arrow, "=>"},
                               {number, "10"},
                               {comma, ","},
                               {identifier, "_"},
                               {fat_arrow, "=>"},
                               {number, "20"},
                               {right_brace, "}"},
                               {semicolon, ";"},
                               {eof, ""},
                           }) &&
             passed;

    passed = expect_tokens("export enum Option<T> { Some(T), None, }",
                           {
                               {export_keyword, "export"},
                               {enum_keyword, "enum"},
                               {identifier, "Option"},
                               {less, "<"},
                               {identifier, "T"},
                               {greater, ">"},
                               {left_brace, "{"},
                               {identifier, "Some"},
                               {left_paren, "("},
                               {identifier, "T"},
                               {right_paren, ")"},
                               {comma, ","},
                               {identifier, "None"},
                               {comma, ","},
                               {right_brace, "}"},
                               {eof, ""},
                           }) &&
             passed;

    return passed ? 0 : 1;
}
