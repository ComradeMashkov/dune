#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp"

#include <filesystem>
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

void check_fixture_source(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::TypeChecker checker;
    checker.check(loader.resolve(parser.parse(), std::filesystem::current_path().parent_path() / "tests" / "fixtures"));
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

bool expect_fixture_valid(const std::string& source, const char* message) {
    try {
        check_fixture_source(source);
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

bool expect_fixture_error_contains(const std::string& source, const std::string& expected, const char* message) {
    try {
        check_fixture_source(source);
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

    passed = expect_valid("func add(a: int, b: int): int { return a + b; } "
                          "var total: int = add(10, 20); var done: bool = total == 30; print(done);",
                          "expected typed functions to validate") &&
             passed;
    passed = expect_valid("func widen(value: u64): u64 { return value + 1; } "
                          "var amount: uint64 = widen(41); var ratio: real = 1 + 2.5; var mark: glyph = 'x';",
                          "expected extended types to validate") &&
             passed;
    passed = expect_valid("func log(message: text): unit { print(message); return; } "
                          "func noop(): unit { } "
                          "var tiny: i8 = 127; var small: i16 = 32767; var mid: i32 = 2147483647; "
                          "var wide: i64 = 9000000000; var index: usize = 5; var offset: isize = 6; "
                          "var rough: real32 = 1 + 2.5; var exact: real64 = 2.5; "
                          "var same: bool = \"done\" == \"done\"; log(\"done\"); noop();",
                          "expected standard scalar types to validate") &&
             passed;
    passed = expect_valid("import math; "
                          "func second(values: [int]): int { return values[1]; } "
                          "var values: [int] = [1, math.square(2)]; values.push(9); "
                          "var count: int = values.len(); var value: int = second(values); "
                          "var exact: real64 = math.square(1.5); var base: u64 = 7; var wide: u64 = math.square(base);",
                          "expected arrays and modules to validate") &&
             passed;
    passed = expect_valid("import math; var rough: real32 = 1.5; var exact: real64 = math.abs(0.0 - 2.5); "
                          "var small: i16 = math.clamp(12, 0, 10); var wide: u64 = 9; "
                          "var high: u64 = math.max(wide, 2); var low: int = math.min(4, 9); "
                          "var cubed: int = math.cube(3); var pi: real64 = math.PI; "
                          "var pi32: real32 = math.PI32; var tau: real64 = math.TAU; var e: real64 = math.E; "
                          "var rounded: real64 = math.round(pi); var rooted: real64 = math.sqrt(9.0); "
                          "var raised: real64 = math.pow(2.0, 3); var wave: real64 = math.sin(0.0); "
                          "var turn: real64 = math.cos(0.0); var slope: real64 = math.tan(0.0); "
                          "var grown: real64 = math.exp(0.0); var logged: real64 = math.ln(1.0); "
                          "var low_real: real64 = math.floor(2.9); var high_real: real64 = math.ceil(2.1); "
                          "var wrapped: real64 = math.normalize_radians(tau);",
                          "expected expanded math functions to validate") &&
             passed;
    passed = expect_valid("var rough: real32 = 1.5; var done: bool = rough < 2.5;",
                          "expected real32 literal comparison to validate") &&
             passed;
    passed = expect_valid("var exact: real64 = 17 to real64; var code: int = 'A' to int; "
                          "var letter: glyph = 66 to glyph; var flag: bool = 0 to bool; "
                          "var done: bool = !false && true || (17 % 5 == 2);",
                          "expected casts and operators to validate") &&
             passed;
    passed = expect_valid("var values: [int] = [1, 2]; values.push(3); var last: int = values.pop(); "
                          "var empty: bool = values.is_empty(); values.clear(); "
                          "var message: text = \"dune language\"; var size: int = message.len(); "
                          "var has: bool = message.contains(\"lang\"); "
                          "var starts: bool = message.starts_with(\"dune\"); var blank: bool = \"\".is_empty();",
                          "expected array and text methods to validate") &&
             passed;
    passed = expect_valid("foreign func c_sqrt(value: real64): real64 = \"sqrt\"; "
                          "var root: real64 = c_sqrt(81.0); "
                          "var message: text = \"dune language\"; var first: glyph = message[0]; "
                          "var word: text = message[5:13]; var prefix: text = message[:4]; "
                          "var suffix: text = message[5:]; var values: [int] = [1, 2, 3, 4]; "
                          "var middle: [int] = values[1:3]; "
                          "for var i = 0; i < 3; i = i + 1 { if i == 1 { continue; } break; }",
                          "expected foreign functions, slices, text indexing, and for loops to validate") &&
             passed;
    passed =
        expect_valid("import array; import text; "
                     "var values: [int] = [1, 2, 3]; var reversed: [int] = array.reverse(values); "
                     "var total: int = array.sum(reversed); var has: bool = array.contains(values, 2); "
                     "var ranged: [int] = array.range(2, 5); "
                     "var message: text = \" dune \"; var stripped: text = text.trim(message); "
                     "var ends: bool = text.ends_with(stripped, \"ne\"); "
                     "var where: int = text.index_of(stripped, 'n'); var count: int = text.count(stripped, 'd'); "
                     "var first: int = values.first(); var added: [int] = values.append(4); "
                     "var direct_trim: text = message.trim(); var direct_ends: bool = direct_trim.ends_with(\"ne\"); "
                     "var digit: bool = text.is_digit('7'); var alpha: bool = text.is_alpha('x');",
                     "expected array and text stdlib modules to validate") &&
        passed;
    passed = expect_valid("import array; import math; "
                          "func identity<T>(value: T): T { return value; } "
                          "func twice<T is numeric>(value: T): T { return value + value; } "
                          "var number: int = identity(42); var label: text = identity(\"done\"); "
                          "var words: [text] = [\"dune\", \"lang\"]; var first: text = words.reverse().first(); "
                          "var small: u16 = 12; var squared: u16 = math.square(small); "
                          "var rough: real32 = 1.5; var real_square: real32 = math.square(rough); "
                          "var doubled: int = twice(9);",
                          "expected generic functions and stdlib generics to validate") &&
             passed;
    passed = expect_valid("func only_bad<T>(value: T): T { return value + value; } print(only_bad(1));",
                          "expected generic functions to instantiate only used types") &&
             passed;
    passed = expect_valid("record Point { x: real64, y: real64 } "
                          "extend Point { func sum(): real64 { return this.x + this.y; } } "
                          "func make(x: real64, y: real64): Point { return Point { x: x, y: y }; } "
                          "var p: Point = make(1.5, 2.5); var total: real64 = p.sum(); var x: real64 = p.x;",
                          "expected records, fields, literals, and methods to validate") &&
             passed;
    passed = expect_valid("record Box<T> { value: T } "
                          "extend<T> Box<T> { func value_or(default: T): T { return this.value; } } "
                          "func boxed<T>(value: T): Box<T> { return Box { value: value }; } "
                          "var number: Box<int> = boxed(7); var label: Box<text> = boxed(\"done\"); "
                          "var chosen: text = case number.value { 7 : label.value_or(\"bad\"), _ : \"other\", };",
                          "expected generic records and case expressions to validate") &&
             passed;
    passed = expect_valid("choice Maybe<T> { Present(T), Absent, } "
                          "var value: Maybe<int> = Present(42); var missing: Maybe<int> = Absent; "
                          "var chosen: int = case value { Present(x) : x, Absent : 0, }; "
                          "var fallback: int = case missing { Present(x) : x, Absent : 7, }; "
                          "var qualified: Maybe<int> = Maybe.Present(9);",
                          "expected choices and variant case expressions to validate") &&
             passed;
    passed = expect_valid("func same<T is comparable>(left: T, right: T): bool { return left == right; } "
                          "func lower<T is ordered>(left: T, right: T): bool { return left < right; } "
                          "var text_ok: bool = same(\"dune\", \"dune\"); var int_ok: bool = lower(1, 2);",
                          "expected comparable and ordered bounds to validate") &&
             passed;
    passed = expect_valid("import maybe; import outcome; import assert; import collections; "
                          "var maybe_value: maybe.Maybe<int> = maybe.present(42); "
                          "var fallback: int = maybe.absent(0).value_or(7); "
                          "var done: outcome.Outcome<int, text> = outcome.done(maybe_value.value_or(0), \"\"); "
                          "var failed: outcome.Outcome<int, text> = outcome.failed(0, \"bad\"); "
                          "var repeated: [int] = collections.repeat_int(3, 4); "
                          "var same: bool = assert.equals_int(repeated[0], done.value_or(0)); "
                          "var error: text = failed.failure_or(\"absent\");",
                          "expected record-based stdlib modules to validate") &&
             passed;
    passed = expect_fixture_valid("import feature_exports; var answer: int = feature_exports.ANSWER; "
                                  "var value: int = feature_exports.public();",
                                  "expected exported module members to validate") &&
             passed;
    passed = expect_valid("var values: [int] = [];", "expected typed empty array to validate") && passed;
    passed = expect_error_contains("var x: int = true;", "expected type 'int' but got 'bool'",
                                   "expected var type mismatch") &&
             passed;
    passed = expect_error_contains("var x: bool = true; x = 1;", "expected type 'bool' but got 'int'",
                                   "expected assignment type mismatch") &&
             passed;
    passed = expect_error_contains("const x: int = 1; x = 2;", "cannot assign to constant 'x'",
                                   "expected const assignment error") &&
             passed;
    passed = expect_error_contains("print(true + 1);", "expected numeric type but got 'bool'",
                                   "expected invalid binary operation") &&
             passed;
    passed = expect_error_contains("print(1.5 % 1.0);", "expected integer type but got 'real'",
                                   "expected invalid modulo operation") &&
             passed;
    passed = expect_error_contains("print(!1);", "expected type 'bool' but got 'int'", "expected invalid unary not") &&
             passed;
    passed = expect_error_contains("var value: int = \"7\" to int;", "cannot cast from 'text' to 'int'",
                                   "expected invalid cast") &&
             passed;
    passed = expect_error_contains("func bad(): bool { return 1; }", "expected type 'bool' but got 'int'",
                                   "expected return type mismatch") &&
             passed;
    passed = expect_error_contains("func is_done(value: bool): bool { return value; } print(is_done(1));",
                                   "no overload for function 'is_done' with argument types (int)",
                                   "expected call argument mismatch") &&
             passed;
    passed =
        expect_error_contains("var too_big: u8 = 256;", "does not fit in type 'u8'", "expected unsigned range error") &&
        passed;
    passed =
        expect_error_contains("var too_big: i8 = 128;", "does not fit in type 'i8'", "expected signed range error") &&
        passed;
    passed = expect_error_contains("var mark: glyph = 65;", "expected type 'glyph' but got 'int'",
                                   "expected glyph mismatch") &&
             passed;
    passed = expect_error_contains("var message: text = 65;", "expected type 'text' but got 'int'",
                                   "expected text mismatch") &&
             passed;
    passed = expect_error_contains("func bad(): unit { return 1; }", "expected type 'unit' but got 'int'",
                                   "expected unit return mismatch") &&
             passed;
    passed = expect_error_contains("func bad(): int { return; }", "expected type 'int' but got 'unit'",
                                   "expected missing return value mismatch") &&
             passed;
    passed = expect_error_contains("func noop(): unit { } var value = noop();", "variables cannot have type 'unit'",
                                   "expected unit binding mismatch") &&
             passed;
    passed = expect_error_contains("var values: [int] = [1, true];", "expected type 'int' but got 'bool'",
                                   "expected mixed array mismatch") &&
             passed;
    passed = expect_error_contains("var values = [];", "empty array literal needs an array type",
                                   "expected empty array annotation error") &&
             passed;
    passed = expect_error_contains("var values: [int] = [1]; print(math.square(values[0]));",
                                   "undefined variable 'math'", "expected missing math import") &&
             passed;
    passed = expect_error_contains("var values: [int] = [1]; values.push(true);", "expected type 'int' but got 'bool'",
                                   "expected array push type mismatch") &&
             passed;
    passed = expect_error_contains("var values: [int] = [1]; values.contains(1);",
                                   "type '[int]' has no method 'contains'", "expected missing array module method") &&
             passed;
    passed = expect_error_contains("var values: [int] = [1]; values.first();", "type '[int]' has no method 'first'",
                                   "expected missing array import for method") &&
             passed;
    passed = expect_error_contains("var message: text = \"done\"; message.contains(1);",
                                   "expected type 'text' but got 'int'", "expected text method argument mismatch") &&
             passed;
    passed =
        expect_error_contains("break;", "break statement outside loop", "expected break outside loop error") && passed;
    passed =
        expect_error_contains("continue;", "continue statement outside loop", "expected continue outside loop error") &&
        passed;
    passed = expect_error_contains("var message: text = \"done\"; print(message[true]);",
                                   "expected integer index but got 'bool'", "expected text index type error") &&
             passed;
    passed =
        expect_error_contains("var values: [int] = [1, 2]; var part: [int] = values[0:true];",
                              "expected integer slice bound but got 'bool'", "expected array slice bound type error") &&
        passed;
    passed = expect_fixture_error_contains("import feature_exports; print(feature_exports.hidden());",
                                           "module 'feature_exports' does not export 'hidden'",
                                           "expected hidden module function error") &&
             passed;
    passed = expect_fixture_error_contains("import feature_exports; print(feature_exports.HIDDEN);",
                                           "module 'feature_exports' does not export 'HIDDEN'",
                                           "expected hidden module constant error") &&
             passed;
    passed = expect_error_contains("import array; print(array.contains([1, 2], true));",
                                   "no overload for function 'array.contains' with argument types ([int], bool)",
                                   "expected array stdlib mismatch") &&
             passed;
    passed = expect_error_contains("import text; print(text.nope(\"x\"));", "module 'text' does not export 'nope'",
                                   "expected missing text module export") &&
             passed;
    passed = expect_error_contains("import math; print(math.square(true));",
                                   "no overload for function 'math.square' with argument types (bool)",
                                   "expected math.square type mismatch") &&
             passed;
    passed = expect_error_contains("import math; print(math.clamp(1, 2));",
                                   "no overload for function 'math.clamp' with argument types (int, int)",
                                   "expected math.clamp arity mismatch") &&
             passed;
    passed = expect_error_contains("func bad<T is nope>(value: T): T { return value; } print(bad(1));",
                                   "unknown generic bound 'nope'", "expected unknown generic bound error") &&
             passed;
    passed = expect_error_contains("record Point { x: int, y: int } var p: Point = Point { x: 1 };",
                                   "missing field 'y' for record 'Point'", "expected missing record field") &&
             passed;
    passed = expect_error_contains("record Point { x: int } var p: Point = Point { x: true };",
                                   "expected type 'int' but got 'bool'", "expected record field type mismatch") &&
             passed;
    passed = expect_error_contains("record Point { x: int } var p: Point = Point { x: 1 }; print(p.y);",
                                   "record 'Point' has no field 'y'", "expected missing record member") &&
             passed;
    passed = expect_error_contains("func same<T is comparable>(left: T, right: T): bool { return left == right; } "
                                   "var values: [int] = [1]; print(same(values, values));",
                                   "no overload for function 'same' with argument types ([int], [int])",
                                   "expected comparable bound mismatch") &&
             passed;
    passed = expect_error_contains("func invalid<T>(left: T, right: T): bool { return left == right; } "
                                   "var values: [int] = [1]; print(invalid(values, values));",
                                   "while instantiating invalid<T = [int]>", "expected generic instantiation trace") &&
             passed;
    passed = expect_error_contains("var value: int = case 1 { 1 : 10, };", "case expression needs a '_' fallback arm",
                                   "expected case fallback error") &&
             passed;
    passed = expect_error_contains("var value: int = case true { true : 1, _ : false, };",
                                   "expected type 'int' but got 'bool'", "expected case outcome mismatch") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } var value: Maybe = Present(true);",
                                   "expected type 'int' but got 'bool'", "expected choice payload mismatch") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } var value: Maybe = Absent; "
                                   "var chosen: int = case value { Present(x) : x, };",
                                   "case expression does not cover every variant of 'Maybe'",
                                   "expected non-exhaustive choice case error") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } var value: Maybe = Absent; "
                                   "var chosen: int = case value { Present(x) : x, Absent(x) : x, };",
                                   "does not have a payload", "expected unit variant payload pattern error") &&
             passed;
    passed = expect_error_contains("record Box<T> { value: T } var value = Box { value: 1 };",
                                   "generic record literal 'Box' needs an expected type",
                                   "expected generic record inference error") &&
             passed;
    passed = expect_error_contains("import math; print(math.UNKNOWN);", "module 'math' does not export 'UNKNOWN'",
                                   "expected missing module value") &&
             passed;
    passed = expect_error_contains("func choose(value: i64): i64 { return value; } "
                                   "func choose(value: u64): u64 { return value; } print(choose(1));",
                                   "ambiguous overload for function 'choose'", "expected ambiguous overload") &&
             passed;
    passed = expect_error_contains("func same(value: int): int { return value; } "
                                   "func same(value: int): int { return value; }",
                                   "duplicate overload for function 'same'", "expected duplicate overload") &&
             passed;
    passed = expect_error_contains("import time;", "unknown module 'time'", "expected unknown module error") && passed;

    return passed ? 0 : 1;
}
