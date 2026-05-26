#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check_source(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::TypeChecker checker;
    checker.check(parser.parse());
}

bool expect_valid(const std::string& source, const char* message) {
    try {
        check_source(source);
    } catch (const std::runtime_error& error) {
        std::cerr << message << ": " << error.what() << '\n';
        return false;
    }

    return true;
}

bool expect_error_contains(const std::string& source, const std::string& expected, const char* message) {
    try {
        check_source(source);
    } catch (const std::runtime_error& error) {
        const std::string actual = error.what();
        if (actual.find(expected) == std::string::npos) {
            std::cerr << message << ": expected error containing '" << expected << "', got '" << actual << "'\n";
            return false;
        }

        return true;
    }

    std::cerr << message << ": expected type error\n";
    return false;
}

} // namespace

int main() {
    bool passed = true;

    passed = expect_valid("fn add(a: int, b: int) -> int { return a + b; } "
                          "let total: int = add(10, 20); let ok: bool = total == 30; print(ok);",
                          "expected typed functions to validate") &&
             passed;
    passed = expect_valid("fn widen(value: u64) -> u64 { return value + 1; } "
                          "let amount: uint64 = widen(41); let ratio: real = 1 + 2.5; let mark: glyph = 'x';",
                          "expected extended types to validate") &&
             passed;
    passed = expect_valid("fn log(message: text) -> unit { print(message); return; } "
                          "fn noop() -> unit { } "
                          "let tiny: i8 = 127; let small: i16 = 32767; let mid: i32 = 2147483647; "
                          "let wide: i64 = 9000000000; let index: usize = 5; let offset: isize = 6; "
                          "let rough: real32 = 1 + 2.5; let exact: real64 = 2.5; "
                          "let same: bool = \"ok\" == \"ok\"; log(\"ok\"); noop();",
                          "expected standard scalar types to validate") &&
             passed;
    passed = expect_error_contains("let x: int = true;", "expected type 'int' but got 'bool'",
                                   "expected let type mismatch") &&
             passed;
    passed = expect_error_contains("let x: bool = true; x = 1;", "expected type 'bool' but got 'int'",
                                   "expected assignment type mismatch") &&
             passed;
    passed = expect_error_contains("print(true + 1);", "expected numeric type but got 'bool'",
                                   "expected invalid binary operation") &&
             passed;
    passed = expect_error_contains("fn bad() -> bool { return 1; }", "expected type 'bool' but got 'int'",
                                   "expected return type mismatch") &&
             passed;
    passed = expect_error_contains("fn is_ok(value: bool) -> bool { return value; } print(is_ok(1));",
                                   "expected type 'bool' but got 'int'", "expected call argument mismatch") &&
             passed;
    passed =
        expect_error_contains("let too_big: u8 = 256;", "does not fit in type 'u8'", "expected unsigned range error") &&
        passed;
    passed =
        expect_error_contains("let too_big: i8 = 128;", "does not fit in type 'i8'", "expected signed range error") &&
        passed;
    passed = expect_error_contains("let mark: glyph = 65;", "expected type 'glyph' but got 'int'",
                                   "expected glyph mismatch") &&
             passed;
    passed = expect_error_contains("let message: text = 65;", "expected type 'text' but got 'int'",
                                   "expected text mismatch") &&
             passed;
    passed = expect_error_contains("fn bad() -> unit { return 1; }", "expected type 'unit' but got 'int'",
                                   "expected unit return mismatch") &&
             passed;
    passed = expect_error_contains("fn bad() -> int { return; }", "expected type 'int' but got 'unit'",
                                   "expected missing return value mismatch") &&
             passed;
    passed = expect_error_contains("fn noop() -> unit { } let value = noop();", "variables cannot have type 'unit'",
                                   "expected unit binding mismatch") &&
             passed;

    return passed ? 0 : 1;
}
