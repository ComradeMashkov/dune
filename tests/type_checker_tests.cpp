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

    passed = expect_valid("fn add(a: int, b: int): int { return a + b; } "
                          "total: int = add(10, 20); done: bool = total == 30; print(done);",
                          "expected typed functions to validate") &&
             passed;
    passed = expect_valid("fn widen(value: u64): u64 { return value + 1; } "
                          "amount: uint64 = widen(41); ratio: real = 1 + 2.5; mark: glyph = 'x';",
                          "expected extended types to validate") &&
             passed;
    passed = expect_valid("fn log(message: text): unit { print(message); return; } "
                          "fn noop(): unit { } "
                          "tiny: i8 = 127; small: i16 = 32767; mid: i32 = 2147483647; "
                          "wide: i64 = 9000000000; index: usize = 5; offset: isize = 6; "
                          "rough: real32 = 1 + 2.5; exact: real64 = 2.5; "
                          "same: bool = \"done\" == \"done\"; log(\"done\"); noop();",
                          "expected standard scalar types to validate") &&
             passed;
    passed = expect_valid("name: text = \"Dune\"; version: int = 1; print(\"{} v{}\", name, version); "
                          "print(\"bool={}, glyph={}, real={}\", true, 'x', 2.5);",
                          "expected formatted print to validate") &&
             passed;
    passed = expect_error_contains("print(\"{} {}\", 1);", "print format string expects 2 arguments but got 1",
                                   "expected missing print format argument error") &&
             passed;
    passed = expect_error_contains("print(\"{}\", 1, 2);", "print format string expects 1 arguments but got 2",
                                   "expected extra print format argument error") &&
             passed;
    passed =
        expect_error_contains("format: text = \"{}\"; print(format, 1);",
                              "print format string must be a string literal", "expected literal print format error") &&
        passed;
    passed = expect_error_contains("print(\"{name}\", 1);", "invalid print format placeholder",
                                   "expected invalid print placeholder error") &&
             passed;
    passed = expect_valid("import math; "
                          "fn second(values: [int]): int { return values[1]; } "
                          "values: [int] = [1, math.square(2)]; values.push(9); "
                          "count: int = values.len(); value: int = second(values); "
                          "exact: real64 = math.square(1.5); base: u64 = 7; wide: u64 = math.square(base);",
                          "expected arrays and modules to validate") &&
             passed;
    passed = expect_valid("import math; rough: real32 = 1.5; exact: real64 = math.abs(0.0 - 2.5); "
                          "small: i16 = math.clamp(12, 0, 10); wide: u64 = 9; "
                          "high: u64 = math.max(wide, 2); low: int = math.min(4, 9); "
                          "cubed: int = math.cube(3); pi: real64 = math.PI; "
                          "pi32: real32 = math.PI32; tau: real64 = math.TAU; e: real64 = math.E; "
                          "rounded: real64 = math.round(pi); rooted: real64 = math.sqrt(9.0); "
                          "raised: real64 = math.pow(2.0, 3); wave: real64 = math.sin(0.0); "
                          "turn: real64 = math.cos(0.0); slope: real64 = math.tan(0.0); "
                          "grown: real64 = math.exp(0.0); logged: real64 = math.ln(1.0); "
                          "low_real: real64 = math.floor(2.9); high_real: real64 = math.ceil(2.1); "
                          "wrapped: real64 = math.normalize_radians(tau);",
                          "expected expanded math functions to validate") &&
             passed;
    passed = expect_valid("rough: real32 = 1.5; done: bool = rough < 2.5;",
                          "expected real32 literal comparison to validate") &&
             passed;
    passed = expect_valid("exact: real64 = 17 to real64; code: int = 'A' to int; "
                          "letter: glyph = 66 to glyph; flag: bool = 0 to bool; "
                          "done: bool = !false && true || (17 % 5 == 2);",
                          "expected casts and operators to validate") &&
             passed;
    passed = expect_valid("values: [int] = [1, 2]; values.push(3); last: int = values.pop(); "
                          "empty: bool = values.is_empty(); values.clear(); "
                          "message: text = \"dune language\"; size: int = message.len(); "
                          "has: bool = message.contains(\"lang\"); "
                          "starts: bool = message.starts_with(\"dune\"); blank: bool = \"\".is_empty();",
                          "expected array and text methods to validate") &&
             passed;
    passed = expect_valid("foreign fn c_sqrt(value: real64): real64 = \"sqrt\"; "
                          "root: real64 = c_sqrt(81.0); "
                          "message: text = \"dune language\"; first: glyph = message[0]; "
                          "word: text = message[5:13]; prefix: text = message[:4]; "
                          "suffix: text = message[5:]; values: [int] = [1, 2, 3, 4]; "
                          "middle: [int] = values[1:3]; "
                          "for i = 0; i < 3; i = i + 1 { if i == 1 { continue; } break; }",
                          "expected foreign functions, slices, text indexing, and for loops to validate") &&
             passed;
    passed = expect_valid("import array; import text; "
                          "values: [int] = [1, 2, 3]; reversed: [int] = array.reverse(values); "
                          "total: int = array.sum(reversed); has: bool = array.contains(values, 2); "
                          "ranged: [int] = array.range(2, 5); "
                          "message: text = \" dune \"; stripped: text = text.trim(message); "
                          "ends: bool = text.ends_with(stripped, \"ne\"); "
                          "where: int = text.index_of(stripped, 'n'); count: int = text.count(stripped, 'd'); "
                          "first: int = values.first(); added: [int] = values.append(4); "
                          "direct_trim: text = message.trim(); direct_ends: bool = direct_trim.ends_with(\"ne\"); "
                          "digit: bool = text.is_digit('7'); alpha: bool = text.is_alpha('x');",
                          "expected array and text stdlib modules to validate") &&
             passed;
    passed = expect_valid("import array; import math; "
                          "fn identity<T>(value: T): T { return value; } "
                          "fn twice<T is numeric>(value: T): T { return value + value; } "
                          "number: int = identity(42); label: text = identity(\"done\"); "
                          "words: [text] = [\"dune\", \"lang\"]; first: text = words.reverse().first(); "
                          "small: u16 = 12; squared: u16 = math.square(small); "
                          "rough: real32 = 1.5; real_square: real32 = math.square(rough); "
                          "doubled: int = twice(9);",
                          "expected generic functions and stdlib generics to validate") &&
             passed;
    passed = expect_valid("fn only_bad<T>(value: T): T { return value + value; } print(only_bad(1));",
                          "expected generic functions to instantiate only used types") &&
             passed;
    passed = expect_valid("record Point { x: real64, y: real64, "
                          "fn sum(): real64 { return this.x + this.y; } } "
                          "fn make(x: real64, y: real64): Point { return Point { x: x, y: y }; } "
                          "p: Point = make(1.5, 2.5); total: real64 = p.sum(); x: real64 = p.x;",
                          "expected records, fields, literals, and methods to validate") &&
             passed;
    passed = expect_valid("record Box<T> { value: T, fn value_or(default: T): T { return this.value; } } "
                          "fn boxed<T>(value: T): Box<T> { return Box { value: value }; } "
                          "number: Box<int> = boxed(7); label: Box<text> = boxed(\"done\"); "
                          "chosen: text = when number.value { "
                          "is 7 { label.value_or(\"bad\") } is _ { \"other\" } };",
                          "expected generic records and when expressions to validate") &&
             passed;
    passed = expect_valid("record Point { x: real64, y: real64, "
                          "fn new(x: real64, y: real64): Point { return Point { x: x, y: y }; } } "
                          "p: Point = Point.new(1.5, 2.5); x: real64 = p.x;",
                          "expected record constructors to validate") &&
             passed;
    passed = expect_valid("record Box<T> { value: T, "
                          "fn new(value: T): Box<T> { return Box { value: value }; } "
                          "fn get(): T { return this.value; } } "
                          "boxed: Box<int> = Box.new(42); value: int = boxed.get();",
                          "expected generic record constructors to validate") &&
             passed;
    passed = expect_valid("contract Shape { area(): real64; } "
                          "record Circle with Shape { radius: real64, "
                          "fn new(radius: real64): Circle { return Circle { radius: radius }; } "
                          "fn area(): real64 { return 3.0 * this.radius * this.radius; } } "
                          "fn area_of<T is Shape>(shape: T): real64 { return shape.area(); } "
                          "circle: Circle = Circle.new(2.0); area: real64 = area_of(circle);",
                          "expected local contracts and contract bounds to validate") &&
             passed;
    passed = expect_fixture_valid("import object_model_api; "
                                  "counter: object_model_api.Counter = object_model_api.Counter.new(); "
                                  "counter.inc(); value: int = counter.current(); "
                                  "fn area_of<T is object_model_api.Shape>(shape: T): real64 { return shape.area(); } "
                                  "circle: object_model_api.Circle = object_model_api.Circle.new(2.0); "
                                  "area: real64 = area_of(circle);",
                                  "expected exported object model module API to validate") &&
             passed;
    passed = expect_valid("choice Maybe<T> { Present(T), Absent, } "
                          "value: Maybe<int> = Present(42); missing: Maybe<int> = Absent; "
                          "chosen: int = when value { is Present(x) { x } is Absent { 0 } }; "
                          "fallback: int = when missing { is Present(x) { x } is Absent { 7 } }; "
                          "qualified: Maybe<int> = Maybe.Present(9);",
                          "expected choices and variant when expressions to validate") &&
             passed;
    passed = expect_valid("fn same<T is comparable>(left: T, right: T): bool { return left == right; } "
                          "fn lower<T is ordered>(left: T, right: T): bool { return left < right; } "
                          "text_ok: bool = same(\"dune\", \"dune\"); int_ok: bool = lower(1, 2);",
                          "expected comparable and ordered bounds to validate") &&
             passed;
    passed = expect_valid("import maybe; import outcome; import assert; import collections; "
                          "maybe_value: maybe.Maybe<int> = maybe.present(42); "
                          "fallback: int = maybe.absent(0).value_or(7); "
                          "done: outcome.Outcome<int, text> = outcome.done(maybe_value.value_or(0), \"\"); "
                          "failed: outcome.Outcome<int, text> = outcome.failed(0, \"bad\"); "
                          "repeated: [int] = collections.repeat_int(3, 4); "
                          "same: bool = assert.equals_int(repeated[0], done.value_or(0)); "
                          "error: text = failed.failure_or(\"absent\");",
                          "expected record-based stdlib modules to validate") &&
             passed;
    passed = expect_valid("import autograd; "
                          "x: autograd.Value = autograd.variable(2.0); "
                          "y: autograd.Value = x.mul(3.0).add(x.pow(2.0)); "
                          "y.backward(); value: real64 = y.data; gradient: real64 = x.grad; "
                          "active: bool = x.requires_grad;",
                          "expected autograd stdlib module to validate") &&
             passed;
    passed = expect_valid("import matrix; "
                          "vi: matrix.Vector<int> = matrix.vector([1, 2, 3]); "
                          "wi: matrix.Vector<int> = matrix.full(3, 2); "
                          "dot: int = vi.dot(wi); "
                          "zeros: matrix.Vector<u16> = matrix.zeros(3); "
                          "filled: matrix.Matrix<real64> = matrix.full(2, 2, 1.5); "
                          "left: matrix.Matrix<int> = matrix.from_flat(2, 3, [1, 2, 3, 4, 5, 6]); "
                          "right: matrix.Matrix<int> = matrix.from_flat(3, 2, [7, 8, 9, 10, 11, 12]); "
                          "product: matrix.Matrix<int> = left.matmul(right); "
                          "cell: int = product.get(1, 1); total: real64 = filled.sum(); "
                          "shape: [int] = product.shape(); flat: matrix.Vector<int> = product.flatten(); "
                          "clipped: matrix.Vector<int> = vi.rsub(10).clip(0, 9); "
                          "diag: matrix.Matrix<int> = matrix.diagonal(matrix.vector([1, 2, 3])); "
                          "sequence: matrix.Vector<int> = matrix.arange(1, 5); ok: bool = left.can_matmul(right); "
                          "rows: matrix.Matrix<int> = matrix.from_rows([[1, 2], [3, 4]]); "
                          "row_sums: matrix.Vector<int> = rows.sum_rows(); "
                          "column_means: matrix.Vector<real64> = rows.mean_columns(); "
                          "mean: real64 = rows.mean(); det: int = rows.det2(); "
                          "norm: real64 = vi.norm(); distance: real64 = vi.distance(wi); "
                          "outer: matrix.Matrix<int> = vi.outer(wi); "
                          "left_product: matrix.Vector<int> = vi.matmul(right); "
                          "top_outer: matrix.Matrix<int> = matrix.outer(vi, wi);",
                          "expected generic matrix stdlib module to validate") &&
             passed;
    passed = expect_valid("import runtime; fn fail(): unit { runtime.panic(\"boom\"); }",
                          "expected runtime panic helper to validate") &&
             passed;
    passed = expect_valid("import array; "
                          "values: [int] = [1, 2, 3]; total: int = values.sum(); "
                          "prod: int = values.product(); low: int = values.min(); high: int = values.max(); "
                          "first: int = values.prepend(0).first(); last: int = values.append(4).last(); "
                          "middle: [int] = values.concat([4, 5]).slice(1, 4); "
                          "zeros: [u16] = array.zeros(3); ones: [real64] = array.ones(2); "
                          "full: [text] = array.full(2, \"x\"); "
                          "same: bool = middle.equals([2, 3, 4]); flags: [bool] = [true, false]; "
                          "has_any: bool = flags.any(); all_true: bool = flags.all();",
                          "expected expanded array stdlib API to validate") &&
             passed;
    passed = expect_fixture_valid("import feature_exports; answer: int = feature_exports.ANSWER; "
                                  "value: int = feature_exports.public();",
                                  "expected exported module members to validate") &&
             passed;
    passed = expect_valid("const HIDDEN: int = 7; fn hidden(): int { return HIDDEN; } value: int = hidden();",
                          "expected top-level constants to be visible inside functions") &&
             passed;
    passed = expect_valid("x = 1; { x: int = 2; y = x + 1; } print(x); "
                          "total = 0; for i = 0; i < 3; i = i + 1 { total = total + i; } "
                          "choice Maybe { Present(int), Absent, } value: Maybe = Present(5); "
                          "chosen: int = when value { is Present(x) { x } is Absent { 0 } };",
                          "expected lexical scopes and shadowing to validate") &&
             passed;
    passed = expect_valid("record Point { x: int, y: int } "
                          "values: [int] = [1, 2]; values[1] = 9; "
                          "grid: [[int]] = [[1, 2], [3, 4]]; grid[1][0] = 8; "
                          "point: Point = Point { x: 1, y: 2 }; point.x = 7; "
                          "points: [Point] = [Point { x: 3, y: 4 }]; points[0].y = 11;",
                          "expected array, record, and nested assignment targets to validate") &&
             passed;
    passed = expect_valid("values: [int] = [];", "expected typed empty array to validate") && passed;
    passed = expect_error_contains("x: int = true;", "expected type 'int' but got 'bool'",
                                   "expected binding type mismatch") &&
             passed;
    passed = expect_error_contains("x: bool = true; x = 1;", "expected type 'bool' but got 'int'",
                                   "expected assignment type mismatch") &&
             passed;
    passed = expect_error_contains("const x: int = 1; x = 2;", "cannot assign to constant 'x'",
                                   "expected const assignment error") &&
             passed;
    passed = expect_error_contains("const x: int = 1; { x: int = 2; }", "cannot shadow constant 'x'",
                                   "expected const shadowing error") &&
             passed;
    passed = expect_error_contains("for i = 0; i < 1; i = i + 1 { } print(i);", "undefined variable 'i'",
                                   "expected for variable scope error") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Present(1); "
                                   "chosen: int = when value { is Present(x) { x } is Absent { 0 } }; print(x);",
                                   "undefined variable 'x'", "expected when payload scope error") &&
             passed;
    passed = expect_error_contains("const values: [int] = [1]; values[0] = 2;",
                                   "cannot mutate through constant binding 'values'",
                                   "expected const indexed assignment error") &&
             passed;
    passed = expect_valid("const values: [int] = [1]; alias = values; alias[0] = 2;",
                          "expected const array alias mutation to validate") &&
             passed;
    passed = expect_error_contains("print(true + 1);", "expected numeric type but got 'bool'",
                                   "expected invalid binary operation") &&
             passed;
    passed = expect_error_contains("print(1.5 % 1.0);", "expected integer type but got 'real'",
                                   "expected invalid modulo operation") &&
             passed;
    passed = expect_error_contains("print(!1);", "expected type 'bool' but got 'int'", "expected invalid unary not") &&
             passed;
    passed = expect_error_contains("value: int = \"7\" to int;", "cannot cast from 'text' to 'int'",
                                   "expected invalid cast") &&
             passed;
    passed = expect_error_contains("fn bad(): bool { return 1; }", "expected type 'bool' but got 'int'",
                                   "expected return type mismatch") &&
             passed;
    passed = expect_error_contains("fn is_done(value: bool): bool { return value; } print(is_done(1));",
                                   "no overload for function 'is_done' with argument types (int)",
                                   "expected call argument mismatch") &&
             passed;
    passed =
        expect_error_contains("too_big: u8 = 256;", "does not fit in type 'u8'", "expected unsigned range error") &&
        passed;
    passed = expect_error_contains("too_big: i8 = 128;", "does not fit in type 'i8'", "expected signed range error") &&
             passed;
    passed =
        expect_error_contains("mark: glyph = 65;", "expected type 'glyph' but got 'int'", "expected glyph mismatch") &&
        passed;
    passed =
        expect_error_contains("message: text = 65;", "expected type 'text' but got 'int'", "expected text mismatch") &&
        passed;
    passed = expect_error_contains("fn bad(): unit { return 1; }", "expected type 'unit' but got 'int'",
                                   "expected unit return mismatch") &&
             passed;
    passed = expect_error_contains("fn bad(): int { return; }", "expected type 'int' but got 'unit'",
                                   "expected missing return value mismatch") &&
             passed;
    passed = expect_error_contains("fn noop(): unit { } value = noop();", "variables cannot have type 'unit'",
                                   "expected unit binding mismatch") &&
             passed;
    passed = expect_error_contains("values: [int] = [1, true];", "expected type 'int' but got 'bool'",
                                   "expected mixed array mismatch") &&
             passed;
    passed = expect_error_contains("values = [];", "empty array literal needs an array type",
                                   "expected empty array annotation error") &&
             passed;
    passed = expect_error_contains("values: [int] = [1]; print(math.square(values[0]));", "undefined variable 'math'",
                                   "expected missing math import") &&
             passed;
    passed = expect_error_contains("values: [int] = [1]; values.push(true);", "expected type 'int' but got 'bool'",
                                   "expected array push type mismatch") &&
             passed;
    passed = expect_error_contains("values: [int] = [1]; values[0] = true;", "expected type 'int' but got 'bool'",
                                   "expected indexed assignment type mismatch") &&
             passed;
    passed = expect_error_contains("message: text = \"a\"; message[0] = 'b';",
                                   "text values are immutable; cannot assign to text index",
                                   "expected text index assignment error") &&
             passed;
    passed = expect_error_contains("value: int = 1; value[0] = 2;", "expected array assignment target but got 'int'",
                                   "expected non-array index assignment target error") &&
             passed;
    passed =
        expect_error_contains("values: [int] = [1]; values.x = 2;", "expected record assignment target but got '[int]'",
                              "expected non-record member assignment target error") &&
        passed;
    passed = expect_error_contains("import math; math.PI = 1.0;", "cannot assign to module member 'math.PI'",
                                   "expected module member assignment target error") &&
             passed;
    passed = expect_error_contains("values: [int] = [1]; values.contains(1);", "type '[int]' has no method 'contains'",
                                   "expected missing array module method") &&
             passed;
    passed = expect_error_contains("values: [int] = [1]; values.first();", "type '[int]' has no method 'first'",
                                   "expected missing array import for method") &&
             passed;
    passed = expect_error_contains("message: text = \"done\"; message.contains(1);",
                                   "expected type 'text' but got 'int'", "expected text method argument mismatch") &&
             passed;
    passed =
        expect_error_contains("break;", "break statement outside loop", "expected break outside loop error") && passed;
    passed =
        expect_error_contains("continue;", "continue statement outside loop", "expected continue outside loop error") &&
        passed;
    passed = expect_error_contains("message: text = \"done\"; print(message[true]);",
                                   "expected integer index but got 'bool'", "expected text index type error") &&
             passed;
    passed =
        expect_error_contains("values: [int] = [1, 2]; part: [int] = values[0:true];",
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
    passed = expect_error_contains("import matrix; bad = matrix.vector([\"x\"]);",
                                   "no overload for function 'matrix.vector' with argument types ([text])",
                                   "expected matrix numeric bound error") &&
             passed;
    passed = expect_error_contains("import matrix; v = matrix.vector([1, 2]); print(v.data);",
                                   "field 'data' of record 'Vector' is private",
                                   "expected private vector data field error") &&
             passed;
    passed = expect_error_contains("import math; print(math.square(true));",
                                   "no overload for function 'math.square' with argument types (bool)",
                                   "expected math.square type mismatch") &&
             passed;
    passed = expect_error_contains("import math; print(math.clamp(1, 2));",
                                   "no overload for function 'math.clamp' with argument types (int, int)",
                                   "expected math.clamp arity mismatch") &&
             passed;
    passed = expect_error_contains("fn bad<T is nope>(value: T): T { return value; } print(bad(1));",
                                   "unknown generic bound 'nope'", "expected unknown generic bound error") &&
             passed;
    passed = expect_error_contains("record Point { x: int, y: int } p: Point = Point { x: 1 };",
                                   "missing field 'y' for record 'Point'", "expected missing record field") &&
             passed;
    passed = expect_error_contains("record Point { x: int } p: Point = Point { x: true };",
                                   "expected type 'int' but got 'bool'", "expected record field type mismatch") &&
             passed;
    passed = expect_error_contains("record Point { x: int } p: Point = Point { x: 1 }; print(p.y);",
                                   "record 'Point' has no field 'y'", "expected missing record member") &&
             passed;
    passed = expect_error_contains("fn same<T is comparable>(left: T, right: T): bool { return left == right; } "
                                   "values: [int] = [1]; print(same(values, values));",
                                   "no overload for function 'same' with argument types ([int], [int])",
                                   "expected comparable bound mismatch") &&
             passed;
    passed = expect_error_contains("fn invalid<T>(left: T, right: T): bool { return left == right; } "
                                   "values: [int] = [1]; print(invalid(values, values));",
                                   "while instantiating invalid<T = [int]>", "expected generic instantiation trace") &&
             passed;
    passed = expect_error_contains("value: int = when 1 { is 1 { 10 } };", "when expression needs a '_' fallback arm",
                                   "expected when fallback error") &&
             passed;
    passed = expect_error_contains("value: int = when true { is true { 1 } is _ { false } };",
                                   "expected type 'int' but got 'bool'", "expected when outcome mismatch") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Present(true);",
                                   "expected type 'int' but got 'bool'", "expected choice payload mismatch") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Absent; "
                                   "chosen: int = when value { is Present(x) { x } };",
                                   "when expression does not cover every variant of 'Maybe'",
                                   "expected non-exhaustive choice when error") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Absent; "
                                   "chosen: int = when value { is Present(x) { x } is Absent(x) { x } };",
                                   "does not have a payload", "expected unit variant payload pattern error") &&
             passed;
    passed = expect_error_contains("record Box<T> { value: T } value = Box { value: 1 };",
                                   "generic record literal 'Box' needs an expected type",
                                   "expected generic record inference error") &&
             passed;
    passed = expect_error_contains("record Point { x: int, fn new(): int { return 1; } }",
                                   "constructor for record 'Point' must return 'Point'",
                                   "expected constructor return type error") &&
             passed;
    passed = expect_fixture_error_contains("import object_model_api; "
                                           "counter: object_model_api.Counter = object_model_api.Counter.new(); "
                                           "print(counter.value);",
                                           "field 'value' of record 'Counter' is private",
                                           "expected private record field error") &&
             passed;
    passed = expect_fixture_error_contains("import object_model_api; "
                                           "counter: object_model_api.Counter = object_model_api.Counter.new(); "
                                           "counter.reset();",
                                           "method 'reset' of record 'Counter' is private",
                                           "expected private record method error") &&
             passed;
    passed = expect_error_contains("contract Shape { area(): real64; } record Circle with Shape { radius: real64 }",
                                   "record 'Circle' declares contract 'Shape' but is missing method 'area(): real64'",
                                   "expected missing contract method error") &&
             passed;
    passed = expect_error_contains("contract Shape { area(): real64; } "
                                   "record Circle with Shape { fn area(): int { return 1; } }",
                                   "method 'area' for contract 'Shape' expected return type 'real64' but got 'int'",
                                   "expected contract return type error") &&
             passed;
    passed = expect_error_contains("record Canvas { } contract Drawable { draw(canvas: Canvas): unit; } "
                                   "record Pen with Drawable { fn draw(canvas: text): unit { } }",
                                   "method 'draw' for contract 'Drawable' expected parameter 1 type 'Canvas' but got "
                                   "'text'",
                                   "expected contract parameter type error") &&
             passed;
    passed = expect_error_contains("record Circle with Shape { }", "unknown contract 'Shape'",
                                   "expected unknown contract error") &&
             passed;
    passed = expect_error_contains("import math; print(math.UNKNOWN);", "module 'math' does not export 'UNKNOWN'",
                                   "expected missing module value") &&
             passed;
    passed = expect_error_contains("fn choose(value: i64): i64 { return value; } "
                                   "fn choose(value: u64): u64 { return value; } print(choose(1));",
                                   "ambiguous overload for function 'choose'", "expected ambiguous overload") &&
             passed;
    passed = expect_error_contains("fn same(value: int): int { return value; } "
                                   "fn same(value: int): int { return value; }",
                                   "duplicate overload for function 'same'", "expected duplicate overload") &&
             passed;
    passed = expect_error_contains("import time;", "unknown module 'time'", "expected unknown module error") && passed;

    return passed ? 0 : 1;
}
