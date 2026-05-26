#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void check_source(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::TypeChecker checker;
    checker.check(loader.resolve(parser.parse()));
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
    passed = expect_valid("import math; "
                          "fn second(values: [int]) -> int { return values[1]; } "
                          "let values: [int] = [1, math.square(2)]; values.push(9); "
                          "let count: int = values.len(); let value: int = second(values); "
                          "let exact: real64 = math.square(1.5); let base: u64 = 7; let wide: u64 = math.square(base);",
                          "expected arrays and modules to validate") &&
             passed;
    passed = expect_valid("import math; let rough: real32 = 1.5; let exact: real64 = math.abs(0.0 - 2.5); "
                          "let small: i16 = math.clamp(12, 0, 10); let wide: u64 = 9; "
                          "let high: u64 = math.max(wide, 2); let low: int = math.min(4, 9); "
                          "let cubed: int = math.cube(3); let pi: real64 = math.PI; "
                          "let pi32: real32 = math.PI32; let tau: real64 = math.TAU; let e: real64 = math.E; "
                          "let rounded: real64 = math.round(pi); let rooted: real64 = math.sqrt(9.0); "
                          "let raised: real64 = math.pow(2.0, 3); let wave: real64 = math.sin(0.0); "
                          "let turn: real64 = math.cos(0.0); let slope: real64 = math.tan(0.0); "
                          "let grown: real64 = math.exp(0.0); let logged: real64 = math.ln(1.0); "
                          "let low_real: real64 = math.floor(2.9); let high_real: real64 = math.ceil(2.1); "
                          "let wrapped: real64 = math.normalize_radians(tau);",
                          "expected expanded math functions to validate") &&
             passed;
    passed = expect_valid("let rough: real32 = 1.5; let ok: bool = rough < 2.5;",
                          "expected real32 literal comparison to validate") &&
             passed;
    passed = expect_valid("let values: [int] = [];", "expected typed empty array to validate") && passed;
    passed = expect_error_contains("let x: int = true;", "expected type 'int' but got 'bool'",
                                   "expected let type mismatch") &&
             passed;
    passed = expect_error_contains("let x: bool = true; x = 1;", "expected type 'bool' but got 'int'",
                                   "expected assignment type mismatch") &&
             passed;
    passed = expect_error_contains("const x: int = 1; x = 2;", "cannot assign to constant 'x'",
                                   "expected const assignment error") &&
             passed;
    passed = expect_error_contains("print(true + 1);", "expected numeric type but got 'bool'",
                                   "expected invalid binary operation") &&
             passed;
    passed = expect_error_contains("fn bad() -> bool { return 1; }", "expected type 'bool' but got 'int'",
                                   "expected return type mismatch") &&
             passed;
    passed = expect_error_contains("fn is_ok(value: bool) -> bool { return value; } print(is_ok(1));",
                                   "no overload for function 'is_ok' with argument types (int)",
                                   "expected call argument mismatch") &&
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
    passed = expect_error_contains("let values: [int] = [1, true];", "expected type 'int' but got 'bool'",
                                   "expected mixed array mismatch") &&
             passed;
    passed = expect_error_contains("let values = [];", "empty array literal needs an array type",
                                   "expected empty array annotation error") &&
             passed;
    passed = expect_error_contains("let values: [int] = [1]; print(math.square(values[0]));",
                                   "undefined variable 'math'", "expected missing math import") &&
             passed;
    passed = expect_error_contains("let values: [int] = [1]; values.push(true);", "expected type 'int' but got 'bool'",
                                   "expected array push type mismatch") &&
             passed;
    passed = expect_error_contains("import math; print(math.square(true));",
                                   "no overload for function 'math.square' with argument types (bool)",
                                   "expected math.square type mismatch") &&
             passed;
    passed = expect_error_contains("import math; print(math.clamp(1, 2));",
                                   "no overload for function 'math.clamp' with argument types (int, int)",
                                   "expected math.clamp arity mismatch") &&
             passed;
    passed = expect_error_contains("import math; print(math.UNKNOWN);", "module 'math' has no value 'UNKNOWN'",
                                   "expected missing module value") &&
             passed;
    passed = expect_error_contains("fn choose(value: i64) -> i64 { return value; } "
                                   "fn choose(value: u64) -> u64 { return value; } print(choose(1));",
                                   "ambiguous overload for function 'choose'", "expected ambiguous overload") &&
             passed;
    passed = expect_error_contains("fn same(value: int) -> int { return value; } "
                                   "fn same(value: int) -> int { return value; }",
                                   "duplicate overload for function 'same'", "expected duplicate overload") &&
             passed;
    passed = expect_error_contains("import time;", "unknown module 'time'", "expected unknown module error") && passed;

    return passed ? 0 : 1;
}
