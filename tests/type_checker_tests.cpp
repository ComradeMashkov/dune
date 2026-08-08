#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "typechecker/type_checker.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string test_source(const std::string& source) {
    return "import io; import fmt; " + source;
}

void check_source(const std::string& source) {
    dune::Lexer lexer(test_source(source));
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::TypeChecker checker;
    checker.check(loader.resolve(parser.parse()));
}

void check_fixture_source(const std::string& source) {
    const std::filesystem::path fixtures = std::filesystem::path(DUNE_FIXTURES_PATH);
    dune::Lexer lexer(test_source(source));
    dune::Parser parser(lexer.tokenize());
    // Importable module fixtures now live under category subfolders. Search the
    // ones that hold them, plus the standard library so their nested imports and
    // any stdlib imports in the test source still resolve.
    dune::ModuleLoader loader({
        std::filesystem::path(DUNE_STDLIB_PATH),
        fixtures / "types",
        fixtures / "stdlib",
    });
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
                          "total: int = add(10, 20); done: bool = total == 30; io.println(done);",
                          "expected typed functions to validate") &&
             passed;
    passed = expect_valid("fn widen(value: u64): u64 { return value + 1; } "
                          "amount: uint64 = widen(41); ratio: real = 1 + 2.5; mark: glyph = 'x';",
                          "expected extended types to validate") &&
             passed;
    passed = expect_valid("fn log(message: text): unit { io.println(message); return; } "
                          "fn noop(): unit { } "
                          "tiny: i8 = 127; small: i16 = 32767; mid: i32 = 2147483647; "
                          "wide: i64 = 9000000000; index: usize = 5; offset: isize = 6; "
                          "rough: real32 = 1 + 2.5; exact: real64 = 2.5; "
                          "same: bool = \"done\" == \"done\"; log(\"done\"); noop();",
                          "expected standard scalar types to validate") &&
             passed;
    passed = expect_valid("name: text = \"Dune\"; version: int = 1; "
                          "io.println(fmt.format(\"{} v{}\", name, version)); "
                          "io.println(fmt.format(\"bool={}, glyph={}, real={}\", true, 'x', 2.5));",
                          "expected formatted print to validate") &&
             passed;
    passed = expect_valid("name: text = \"Dune\"; version: int = 1; "
                          "message: text = fmt.format(\"{} v{}\", name, version); "
                          "io.println(fmt.format(\"{}: {}\", \"answer\", 42)); io.println(message);",
                          "expected format expression to validate") &&
             passed;
    passed = expect_valid("size: int = 1_000_000; mask: u64 = 0xffu64; bits: u8 = 0b1010_0101u8; "
                          "wide: i64 = 123i64; index: usize = 10usize; rough: real = 1_000.5_25;",
                          "expected numeric literal polish to validate") &&
             passed;
    passed = expect_valid("choice Shape { Circle(int), Named(text), Empty, } "
                          "a: Shape = Circle(5); b: Shape = Empty; "
                          "io.println(a); io.println(fmt.format(\"{}\", b));",
                          "expected choices with scalar payloads to print by default") &&
             passed;
    passed = expect_valid("choice Maybe2<T> { Some(T), None, } "
                          "m: Maybe2<int> = Some(7); io.println(m);",
                          "expected generic choice with printable argument to print") &&
             passed;
    passed = expect_error_contains("record Point { x: int } choice Wrap { Boxed(Point), Nothing, } "
                                   "w: Wrap = Boxed(Point { x: 1 }); io.println(w);",
                                   "choices print by default only when every variant payload is a scalar or text",
                                   "expected choice with record payload to be rejected") &&
             passed;
    passed = expect_error_contains("record Point { x: int } choice Maybe2<T> { Some(T), None, } "
                                   "m: Maybe2<Point> = Some(Point { x: 1 }); io.println(m);",
                                   "cannot format type 'Maybe2<Point>'",
                                   "expected generic choice with record argument to be rejected") &&
             passed;
    passed =
        expect_valid(
            R"dune(path: text = r"C:\Users\name\data.csv"; literal: text = r"\x"; line: text = "hello\n"; tab: glyph = '\t'; newline: glyph = '\n'; carriage: glyph = '\r'; quote: glyph = '\''; slash: glyph = '\\'; zero: glyph = '\0';)dune",
            "expected raw strings and escaped literals to validate") &&
        passed;
    passed = expect_error_contains(R"(bad: text = "\x";)", R"(unknown text escape '\x')",
                                   "expected invalid text escape error") &&
             passed;
    passed = expect_error_contains(R"(bad: glyph = '\x';)", R"(unknown glyph escape '\x')",
                                   "expected invalid glyph escape error") &&
             passed;
    passed = expect_error_contains("print(1);", "print is not available globally",
                                   "expected bare print to require io import and qualification") &&
             passed;
    passed = expect_error_contains("format(\"{}\", 1);", "format is not available globally",
                                   "expected bare format to require fmt import and qualification") &&
             passed;
    passed = expect_valid("fn print(value: int): unit { return; } print(1);",
                          "expected user-defined print function to validate") &&
             passed;
    passed = expect_valid("fn format(value: int): text { return \"#\"; } label: text = format(1);",
                          "expected user-defined format function to validate") &&
             passed;
    passed = expect_error_contains("fmt.format(\"{} {}\", 1);", "format string expects 2 arguments but got 1",
                                   "expected missing format argument error") &&
             passed;
    passed = expect_error_contains("fmt.format(\"{}\", 1, 2);", "format string expects 1 arguments but got 2",
                                   "expected extra format argument error") &&
             passed;
    passed = expect_error_contains("format: text = \"{}\"; fmt.format(format, 1);",
                                   "format string must be a string literal", "expected literal format error") &&
             passed;
    passed = expect_error_contains("fmt.format(\"{name}\", 1);", "invalid format placeholder",
                                   "expected invalid format placeholder error") &&
             passed;
    passed = expect_error_contains("fmt.format(\"{} {}\", 1);", "format string expects 2 arguments but got 1",
                                   "expected missing format argument error") &&
             passed;
    passed = expect_error_contains("fmt.format(\"{}\", 1, 2);", "format string expects 1 arguments but got 2",
                                   "expected extra format argument error") &&
             passed;
    passed = expect_error_contains("format: text = \"{}\"; message: text = fmt.format(format, 1);",
                                   "format string must be a string literal", "expected literal format error") &&
             passed;
    passed = expect_error_contains("value: i32 = 42u64;", "expected type 'i32' but got 'u64'",
                                   "expected suffixed integer type mismatch") &&
             passed;
    passed = expect_error_contains("small: u8 = 300u8;", "integer literal '300u8' does not fit in type 'u8'",
                                   "expected suffixed integer overflow") &&
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
    passed = expect_valid("fn only_bad<T>(value: T): T { return value + value; } io.println(only_bad(1));",
                          "expected generic functions to instantiate only used types") &&
             passed;
    passed = expect_valid("type Count = int; type Counts = [Count]; "
                          "fn inc(value: Count): Count { return value + 1; } "
                          "values: Counts = [inc(1), 3]; total: Count = values[0] + values[1];",
                          "expected primitive and array type aliases to validate") &&
             passed;
    passed = expect_valid("record Point { x: real64, y: real64, "
                          "fn sum(): real64 { return this.x + this.y; } } "
                          "fn make(x: real64, y: real64): Point { return Point { x: x, y: y }; } "
                          "p: Point = make(1.5, 2.5); total: real64 = p.sum(); x: real64 = p.x;",
                          "expected records, fields, literals, and methods to validate") &&
             passed;
    passed = expect_valid("record Point { x: int, fn new(x: int): Point { return Point { x: x }; } } "
                          "type PointAlias = Point; "
                          "fn shift(point: PointAlias): PointAlias { return Point.new(point.x + 1); } "
                          "point: PointAlias = shift(Point.new(2)); x: int = point.x;",
                          "expected record type aliases to validate") &&
             passed;
    passed = expect_valid("record Optimizer { lr: real64 = 0.01, momentum: real64 = 0.0, name: text = \"sgd\" } "
                          "base: Optimizer = Optimizer {}; fast: Optimizer = Optimizer { lr: 0.1 }; "
                          "label: text = fast.name;",
                          "expected record field defaults to validate") &&
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
    passed = expect_valid("record Optimizer { lr: real64, momentum: real64, "
                          "static fn default(): Optimizer { return Optimizer { lr: 0.01, momentum: 0.0 }; } "
                          "static fn with_lr(lr: real64): Optimizer { return Optimizer { lr: lr, momentum: 0.0 }; } "
                          "fn rate(): real64 { return this.lr; } } "
                          "opt: Optimizer = Optimizer.default(); tuned: Optimizer = Optimizer.with_lr(0.2); "
                          "rate: real64 = tuned.rate();",
                          "expected static associated record functions to validate") &&
             passed;
    passed = expect_valid("record Box<T> { value: T, "
                          "static fn wrap(value: T): Box<T> { return Box { value: value }; } "
                          "fn get(): T { return this.value; } } boxed: Box<int> = Box.wrap(42); "
                          "value: int = boxed.get();",
                          "expected generic static associated record function to validate") &&
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
                                  "zero: object_model_api.Counter = object_model_api.Counter.zero(); "
                                  "fn area_of<T is object_model_api.Shape>(shape: T): real64 { return shape.area(); } "
                                  "circle: object_model_api.Circle = object_model_api.Circle.new(2.0); "
                                  "area: real64 = area_of(circle);",
                                  "expected exported object model module API to validate") &&
             passed;
    passed = expect_fixture_valid("import type_aliases; "
                                  "point: type_aliases.PointAlias = type_aliases.Point.new(7); "
                                  "points: type_aliases.PointList = [point]; "
                                  "again: type_aliases.PointAlias = points[0]; "
                                  "box: type_aliases.BoxAlias<int> = type_aliases.Box.wrap(42); "
                                  "pair: type_aliases.Pair<int, text> = (box.value, \"answer\");",
                                  "expected exported module type aliases to validate") &&
             passed;
    passed = expect_valid("import matrix; import outcome; "
                          "record Box<T> { value: T, static fn wrap(value: T): Box<T> { return Box { value: value }; } } "
                          "type BoxOf<T> = Box<T>; type BoxAgain<T> = BoxOf<T>; "
                          "type Pair<T, U> = (T, U); type TextResult<T> = outcome.Outcome<T, text>; "
                          "type Matrix2<T> = matrix.Matrix<T, 2, 2>; "
                          "box: BoxAgain<int> = Box.wrap(42); pair: Pair<int, text> = (box.value, \"answer\"); "
                          "(answer, label) = pair; result: TextResult<int> = outcome.done_int(answer); "
                          "matrix_value: Matrix2<int> = matrix.identity(2);",
                          "expected generic aliases for records, tuples, modules, alias chains, and static shapes") &&
             passed;
    passed = expect_valid("choice Maybe<T> { Present(T), Absent, } "
                          "value: Maybe<int> = Present(42); missing: Maybe<int> = Absent; "
                          "chosen: int = when value { is Present(x) { x } is Absent { 0 } }; "
                          "fallback: int = when missing { is Present(x) { x } is Absent { 7 } }; "
                          "qualified: Maybe<int> = Maybe.Present(9);",
                          "expected choices and variant when expressions to validate") &&
             passed;
    passed = expect_valid("choice Maybe { Present(int), Absent, } "
                          "value: Maybe = Present(42); missing: Maybe = Absent; "
                          "chosen: int = when value { Present(x) => x; Absent => 0; }; "
                          "fallback: int = when missing { Present(x) => x, _ => 7 };",
                          "expected arrow-style choice when expressions to validate") &&
             passed;
    passed = expect_valid("flag: bool = true; "
                          "label: text = when flag { true => \"enabled\"; false => \"disabled\"; };",
                          "expected exhaustive boolean when expression without fallback to validate") &&
             passed;
    passed = expect_valid("record Point { x: int, y: int } "
                          "fn minmax(): (int, int) { return (2, 5); } "
                          "(lo, hi) = minmax(); "
                          "point: Point = Point { x: lo, y: hi }; "
                          "sum: int = when point { Point { x, y } => x + y; }; "
                          "pair_sum: int = when (lo, hi) { (left, right) => left + right; };",
                          "expected tuples and destructuring patterns to validate") &&
             passed;
    passed = expect_valid("fn same<T is comparable>(left: T, right: T): bool { return left == right; } "
                          "fn lower<T is ordered>(left: T, right: T): bool { return left < right; } "
                          "text_ok: bool = same(\"dune\", \"dune\"); int_ok: bool = lower(1, 2);",
                          "expected comparable and ordered bounds to validate") &&
             passed;
    passed = expect_valid("fn spans<T is ordered + comparable>(a: T, b: T): bool { return (a < b) == (a == b); } "
                          "ok: bool = spans(1, 2);",
                          "expected grouped generic bounds (T is A + B) to validate") &&
             passed;
    passed = expect_valid("fn spans<T is ordered, T is comparable>(a: T, b: T): bool { return (a < b) == (a == b); } "
                          "ok: bool = spans(1, 2);",
                          "expected repeated generic bounds (T is A, T is B) to validate") &&
             passed;
    passed = expect_valid("record Vec2 { x: int, y: int, "
                          "fn add(other: Vec2): Vec2 { return Vec2 { x: this.x + other.x, y: this.y + other.y }; } "
                          "fn mul(factor: int): Vec2 { return Vec2 { x: this.x * factor, y: this.y * factor }; } } "
                          "a: Vec2 = Vec2 { x: 1, y: 2 }; b: Vec2 = Vec2 { x: 3, y: 4 }; "
                          "sum: Vec2 = a + b; scaled: Vec2 = a * 5;",
                          "expected operator overloading via add/mul methods to validate") &&
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
    passed = expect_valid("x = 1; { x: int = 2; y = x + 1; } io.println(x); "
                          "total = 0; for i = 0; i < 3; i = i + 1 { total = total + i; } "
                          "choice Maybe { Present(int), Absent, } value: Maybe = Present(5); "
                          "chosen: int = when value { is Present(x) { x } is Absent { 0 } };",
                          "expected lexical scopes and shadowing to validate") &&
             passed;
    passed = expect_valid("values: [int] = [1, 2, 3]; total = 0; "
                          "for value in values { total = total + value; } "
                          "for i in 0..values.len() { total = total + values[i]; }",
                          "expected for-in arrays and ranges to validate") &&
             passed;
    passed = expect_valid("import set; import matrix; "
                          "seen: set.Set = set.Set.new(); seen.add(\"a\"); seen.add(\"bb\"); "
                          "set_total = 0; for value in seen { set_total = set_total + value.len(); } "
                          "values = matrix.arange(1, 4); vector_total = 0; "
                          "for value in values { vector_total = vector_total + value; } "
                          "doubled: [int] = [value * 2 for value in values];",
                          "expected known record iterables to validate") &&
             passed;
    passed = expect_valid("record Point { x: int, y: int } "
                          "values: [int] = [1, 2]; values[1] = 9; "
                          "grid: [[int]] = [[1, 2], [3, 4]]; grid[1][0] = 8; "
                          "point: Point = Point { x: 1, y: 2 }; point.x = 7; "
                          "points: [Point] = [Point { x: 3, y: 4 }]; points[0].y = 11;",
                          "expected array, record, and nested assignment targets to validate") &&
             passed;
    passed = expect_valid("values: [int] = [];", "expected typed empty array to validate") && passed;
    passed = expect_valid("values: [int] = [1, 2, 3]; found: bool = 2 in values; "
                          "message: text = \"dune language\"; has: bool = \"lang\" in message; "
                          "enabled: bool = true; ok: bool = 1 + 1 in values && enabled;",
                          "expected membership operator to validate") &&
             passed;
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
    passed = expect_error_contains("for i = 0; i < 1; i = i + 1 { } io.println(i);", "undefined variable 'i'",
                                   "expected for variable scope error") &&
             passed;
    passed = expect_error_contains("for value in 42 { io.println(value); }", "type 'int' is not iterable",
                                   "expected non-iterable for-in error") &&
             passed;
    passed = expect_error_contains("import io; import dict; scores: dict.Dict<int> = dict.Dict.new(); "
                                   "for entry in scores { io.println(entry); }",
                                   "type 'dict.Dict<int>' is not iterable",
                                   "expected direct dict iteration to stay explicit") &&
             passed;
    passed = expect_error_contains("for value in [1, 2] { value = value + 1; }", "cannot assign to constant 'value'",
                                   "expected read-only for-in binding") &&
             passed;
    passed = expect_error_contains("value = 0..3;", "range expressions can only be used in for-in loops",
                                   "expected range value error") &&
             passed;
    passed = expect_error_contains("for i in 0.0..3.0 { io.println(i); }", "expected integer range bound but got 'real'",
                                   "expected real range bound error") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Present(1); "
                                   "chosen: int = when value { is Present(x) { x } is Absent { 0 } }; io.println(x);",
                                   "undefined variable 'x'", "expected when payload scope error") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Present(1); "
                                   "chosen: int = when value { Present(x) => x; Absent => 0; }; io.println(x);",
                                   "undefined variable 'x'", "expected arrow when payload scope error") &&
             passed;
    passed = expect_valid("record Box { value: int } "
                          "const values: [int] = [1]; values[0] = 2; values.push(3); "
                          "const box: Box = Box { value: 1 }; box.value = 2;",
                          "expected mutation through const bindings to validate") &&
             passed;
    passed = expect_valid("const values: [int] = [1]; "
                          "fn update(): unit { values[0] = 2; values.push(3); } update();",
                          "expected global const aggregate mutation inside a function to validate") &&
             passed;
    passed = expect_valid("const values: [int] = [1]; alias = values; alias[0] = 2;",
                          "expected const array alias mutation to validate") &&
             passed;
    passed = expect_valid("record Counter { value: int, fn increment(): unit { this.value = this.value + 1; } } "
                          "const counter: Counter = Counter { value: 0 }; counter.increment();",
                          "expected const receiver mutation to validate") &&
             passed;
    passed = expect_error_contains(
                 "record Box { value: int, fn replace(): unit { this = Box { value: 2 }; } }",
                 "cannot reassign method receiver 'this'", "expected method receiver reassignment error") &&
             passed;
    passed = expect_valid("fn replace(this: int): int { this = 2; return this; } value = replace(1);",
                          "expected an ordinary parameter named this to remain reassignable") &&
             passed;
    passed = expect_error_contains("method<T> [T].replace(): unit { this = []; } values = [1]; values.replace();",
                                   "cannot reassign method receiver 'this'",
                                   "expected extension method receiver reassignment error") &&
             passed;
    passed = expect_error_contains("io.println(true + 1);", "expected numeric type but got 'bool'",
                                   "expected invalid binary operation") &&
             passed;
    passed = expect_valid("io.println(\"foo\" + \"bar\"); io.println(\"count \" + '5');",
                          "expected text concatenation to validate") &&
             passed;
    passed = expect_error_contains("io.println(\"n = \" + 1);", "cannot concatenate 'text' with 'int'",
                                   "expected invalid text concatenation") &&
             passed;
    passed = expect_error_contains("io.println(1.5 % 1.0);", "expected integer type but got 'real'",
                                   "expected invalid modulo operation") &&
             passed;
    passed = expect_error_contains("io.println(!1);", "expected type 'bool' but got 'int'", "expected invalid unary not") &&
             passed;
    passed = expect_error_contains("value: int = \"7\" to int;", "cannot cast from 'text' to 'int'",
                                   "expected invalid cast") &&
             passed;
    passed = expect_error_contains("fn bad(): bool { return 1; }", "expected type 'bool' but got 'int'",
                                   "expected return type mismatch") &&
             passed;
    passed = expect_error_contains("fn is_done(value: bool): bool { return value; } io.println(is_done(1));",
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
    passed = expect_error_contains("type Count = int; type Count = text;", "duplicate type alias 'Count'",
                                   "expected duplicate type alias error") &&
             passed;
    passed = expect_error_contains("type MissingAlias = Missing;", "unknown type 'Missing'",
                                   "expected unknown type alias target error") &&
             passed;
    passed = expect_error_contains("type A = B; type B = A;", "cyclic type alias involving 'B'",
                                   "expected cyclic type alias error") &&
             passed;
    passed = expect_error_contains("type Count = int; value: Count<text> = 1;",
                                   "type alias 'Count' does not take type arguments",
                                   "expected non-generic type alias argument error") &&
             passed;
    passed = expect_error_contains("type Box<T> = [T]; value: Box = [1];",
                                   "type alias 'Box' expects 1 type argument(s) but got 0",
                                   "expected missing generic type alias argument error") &&
             passed;
    passed = expect_error_contains("type Pair<T, U> = (T, U); value: Pair<int> = (1, 2);",
                                   "type alias 'Pair' expects 2 type argument(s) but got 1",
                                   "expected generic type alias arity error") &&
             passed;
    passed = expect_error_contains("type Box<T> = [T]; value: Box<3> = [1, 2, 3];",
                                   "type alias 'Box' expects a type argument for 'T'",
                                   "expected const argument rejection for a type alias parameter") &&
             passed;
    passed = expect_error_contains("type Broken<T> = [U];", "unknown type 'U'",
                                   "expected unknown generic alias target parameter error") &&
             passed;
    passed = expect_error_contains("type A<T> = B<T>; type B<U> = A<U>;",
                                   "cyclic type alias involving 'B'",
                                   "expected generic type alias cycle error") &&
             passed;
    passed = expect_error_contains("type Numbers<T is numeric> = [T];",
                                   "generic type alias parameter 'T' cannot have bounds yet",
                                   "expected bounded generic alias parameter error") &&
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
    passed =
        expect_error_contains("bad: bool = 1 in 2;", "operator 'in' requires array or text container but got 'int'",
                              "expected unsupported membership container error") &&
        passed;
    passed = expect_error_contains("values: [int] = [1, 2]; bad: bool = true in values;",
                                   "expected type 'int' but got 'bool'", "expected membership value mismatch") &&
             passed;
    passed = expect_error_contains("record Point { x: int } values: [Point] = []; "
                                   "p: Point = Point { x: 1 }; bad: bool = p in values;",
                                   "operator 'in' requires comparable array elements but got 'Point'",
                                   "expected non-comparable membership error") &&
             passed;
    passed = expect_error_contains("values: [int] = [1]; io.println(math.square(values[0]));", "undefined variable 'math'",
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
    passed = expect_error_contains("message: text = \"done\"; io.println(message[true]);",
                                   "expected integer index but got 'bool'", "expected text index type error") &&
             passed;
    passed =
        expect_error_contains("values: [int] = [1, 2]; part: [int] = values[0:true];",
                              "expected integer slice bound but got 'bool'", "expected array slice bound type error") &&
        passed;
    passed = expect_fixture_error_contains("import feature_exports; io.println(feature_exports.hidden());",
                                           "module 'feature_exports' does not export 'hidden'",
                                           "expected hidden module function error") &&
             passed;
    passed = expect_fixture_error_contains("import feature_exports; io.println(feature_exports.HIDDEN);",
                                           "module 'feature_exports' does not export 'HIDDEN'",
                                           "expected hidden module constant error") &&
             passed;
    passed = expect_error_contains("import array; io.println(array.contains([1, 2], true));",
                                   "no overload for function 'array.contains' with argument types ([int], bool)",
                                   "expected array stdlib mismatch") &&
             passed;
    passed = expect_error_contains("import text; io.println(text.nope(\"x\"));", "module 'text' does not export 'nope'",
                                   "expected missing text module export") &&
             passed;
    passed = expect_error_contains("import matrix; bad = matrix.vector([\"x\"]);",
                                   "no overload for function 'matrix.vector' with argument types ([text])",
                                   "expected matrix numeric bound error") &&
             passed;
    passed = expect_error_contains("import matrix; v = matrix.vector([1, 2]); io.println(v.data);",
                                   "field 'data' of record 'Vector' is private",
                                   "expected private vector data field error") &&
             passed;
    passed = expect_error_contains("import math; io.println(math.square(true));",
                                   "no overload for function 'math.square' with argument types (bool)",
                                   "expected math.square type mismatch") &&
             passed;
    passed = expect_error_contains("import math; io.println(math.clamp(1, 2));",
                                   "no overload for function 'math.clamp' with argument types (int, int)",
                                   "expected math.clamp arity mismatch") &&
             passed;
    passed = expect_error_contains("fn bad<T is nope>(value: T): T { return value; } io.println(bad(1));",
                                   "unknown generic bound 'nope'", "expected unknown generic bound error") &&
             passed;
    passed = expect_error_contains("record Point { x: int, y: int } p: Point = Point { x: 1 };",
                                   "missing field 'y' for record 'Point'", "expected missing record field") &&
             passed;
    passed = expect_error_contains("record Config { required: int, optional: int = 7 } "
                                   "config: Config = Config { optional: 1 };",
                                   "missing field 'required' for record 'Config'",
                                   "expected required record field to remain mandatory") &&
             passed;
    passed =
        expect_error_contains("record Config { value: int = true } config: Config = Config {};",
                              "expected type 'int' but got 'bool'", "expected record default field type mismatch") &&
        passed;
    passed = expect_error_contains("record Config { base: int = 10, doubled: int = base * 2 } "
                                   "config: Config = Config {};",
                                   "undefined variable 'base'", "expected record defaults not to see sibling fields") &&
             passed;
    passed = expect_error_contains("record Point { x: int } p: Point = Point { x: true };",
                                   "expected type 'int' but got 'bool'", "expected record field type mismatch") &&
             passed;
    passed = expect_error_contains("record Point { x: int } p: Point = Point { x: 1 }; io.println(p.y);",
                                   "record 'Point' has no field 'y'", "expected missing record member") &&
             passed;
    passed = expect_error_contains("fn same<T is comparable>(left: T, right: T): bool { return left == right; } "
                                   "values: [int] = [1]; io.println(same(values, values));",
                                   "no overload for function 'same' with argument types ([int], [int])",
                                   "expected comparable bound mismatch") &&
             passed;
    passed = expect_error_contains("fn needs<T is comparable + numeric>(v: T): T { return v; } io.println(needs(\"x\"));",
                                   "does not satisfy bound 'numeric'",
                                   "expected multi-bound violation to name the unmet bound") &&
             passed;
    passed = expect_error_contains("record Point { x: int } p: Point = Point { x: 1 }; q: Point = Point { x: 2 }; "
                                   "bad: Point = p + q;",
                                   "operator '+' is not defined for type 'Point'",
                                   "expected operator-not-defined error for records without the method") &&
             passed;
    passed = expect_error_contains("fn invalid<T>(left: T, right: T): bool { return left == right; } "
                                   "values: [int] = [1]; io.println(invalid(values, values));",
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
                                   "when expression does not cover every variant of 'Maybe'; missing variant: Absent",
                                   "expected non-exhaustive choice when error") &&
             passed;
    passed = expect_error_contains("choice State { Ready, Running(int), Failed(text), } value: State = Ready; "
                                   "label: int = when value { Ready => 0; };",
                                   "missing variants: Running, Failed",
                                   "expected all missing choice variants in declaration order") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Absent; "
                                   "chosen: int = when value { is Present(x) { x } is Absent(x) { x } };",
                                   "does not have a payload", "expected unit variant payload pattern error") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Absent; "
                                   "chosen: int = when value { Missing => 0; _ => 1; };",
                                   "has no variant 'Missing'", "expected unknown arrow variant pattern error") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Absent; "
                                   "chosen: int = when value { Present(x) => x; Present(y) => y; Absent => 0; };",
                                   "duplicate when branch for variant 'Present'",
                                   "expected duplicate arrow variant pattern error") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Absent; "
                                   "chosen: int = when value { Present(x) => x; _ => 0; Absent => 1; };",
                                   "unreachable when branch after '_' fallback",
                                   "expected choice branch after fallback to be unreachable") &&
             passed;
    passed = expect_error_contains("choice Maybe { Present(int), Absent, } value: Maybe = Absent; "
                                   "chosen: int = when value { Present(x) => x; Absent => 0; _ => 1; };",
                                   "unreachable '_' fallback; every variant of 'Maybe' is already covered",
                                   "expected redundant choice fallback to be unreachable") &&
             passed;
    passed = expect_error_contains("value: int = when 1 { 1 => 10; 0x1 => 20; _ => 30; };",
                                   "duplicate when branch for pattern '0x1'",
                                   "expected equivalent integer literal patterns to be duplicates") &&
             passed;
    passed = expect_error_contains("value: int = when 0 { -0 => 10; 0 => 20; _ => 30; };",
                                   "duplicate when branch for pattern '0'",
                                   "expected signed zero integer patterns to be duplicates") &&
             passed;
    passed = expect_error_contains("value: real64 = when 1.0 { 1.0 => 10.0; 1.00 => 20.0; _ => 30.0; };",
                                   "duplicate when branch for pattern '1.00'",
                                   "expected equivalent real literal patterns to be duplicates") &&
             passed;
    passed = expect_error_contains("value: int = when 'a' { 'a' => 1; 'a' => 2; _ => 0; };",
                                   "duplicate when branch for pattern ''a''",
                                   "expected duplicate glyph literal pattern error") &&
             passed;
    passed = expect_error_contains(
                 R"dune(value: int = when "line\\n" { "line\\n" => 1; r"line\n" => 2; _ => 0; };)dune",
                 R"(duplicate when branch for pattern 'r"line\n"')",
                 "expected semantically equivalent escaped and raw text patterns to be duplicates") &&
             passed;
    passed = expect_error_contains("value: int = when 1 { 1 => 10; _ => 20; 2 => 30; };",
                                   "unreachable when branch after '_' fallback",
                                   "expected scalar branch after fallback to be unreachable") &&
             passed;
    passed =
        expect_error_contains("flag: bool = true; value: int = when flag { true => 1; true => 2; _ => 0; };",
                              "duplicate when branch for pattern 'true'", "expected duplicate boolean pattern error") &&
        passed;
    passed = expect_error_contains("flag: bool = true; value: int = when flag { true => 1; false => 0; _ => 2; };",
                                   "unreachable '_' fallback; both boolean values are already covered",
                                   "expected redundant boolean fallback to be unreachable") &&
             passed;
    passed = expect_error_contains("flag: bool = true; value: int = when flag { true => 1; false => 0; flag => 2; };",
                                   "unreachable when branch; both boolean values are already covered",
                                   "expected branch after exhaustive boolean literals to be unreachable") &&
             passed;
    passed = expect_error_contains("pair: (int, int) = (1, 2, 3);", "tuple literal expected 2 elements but got 3",
                                   "expected tuple literal arity error") &&
             passed;
    passed = expect_error_contains("(left, right) = 1;", "expected tuple value but got 'int'",
                                   "expected non-tuple destructuring error") &&
             passed;
    passed =
        expect_error_contains("left: int = 0; right: int = 0; (left, right) = (1, true);",
                              "expected type 'int' but got 'bool'", "expected tuple destructuring type mismatch") &&
        passed;
    passed = expect_error_contains("record Point { x: int, y: int } point: Point = Point { x: 1, y: 2 }; "
                                   "sum: int = when point { Point { z } => z; };",
                                   "record 'Point' has no field 'z'", "expected record pattern field error") &&
             passed;
    passed =
        expect_error_contains("value = (1, 2); sum: int = when value { (left, middle, right) => left; };",
                              "tuple pattern expected 2 elements but got 3", "expected tuple pattern arity error") &&
        passed;
    passed = expect_error_contains("record Box<T> { value: T } value = Box { value: 1 };",
                                   "generic record literal 'Box' needs an expected type",
                                   "expected generic record inference error") &&
             passed;
    passed = expect_error_contains("record Point { x: int, fn new(): int { return 1; } }",
                                   "constructor for record 'Point' must return 'Point'",
                                   "expected constructor return type error") &&
             passed;
    passed = expect_error_contains("record Point { x: int } p: Point = Point.origin();",
                                   "record 'Point' has no static method 'origin'",
                                   "expected missing static associated function error") &&
             passed;
    passed = expect_fixture_error_contains("import object_model_api; "
                                           "counter: object_model_api.Counter = object_model_api.Counter.new(); "
                                           "io.println(counter.value);",
                                           "field 'value' of record 'Counter' is private",
                                           "expected private record field error") &&
             passed;
    passed = expect_fixture_error_contains("import object_model_api; "
                                           "counter: object_model_api.Counter = object_model_api.Counter.new(); "
                                           "counter.reset();",
                                           "method 'reset' of record 'Counter' is private",
                                           "expected private record method error") &&
             passed;
    passed = expect_fixture_error_contains("import object_model_api; "
                                           "counter: object_model_api.Counter = object_model_api.Counter.secret();",
                                           "method 'secret' of record 'Counter' is private",
                                           "expected private static record method error") &&
             passed;
    passed = expect_error_contains("contract Shape { area(): real64; } record Circle with Shape { radius: real64 }",
                                   "record 'Circle' declares contract 'Shape' but is missing method 'area(): real64'",
                                   "expected missing contract method error") &&
             passed;
    passed = expect_error_contains("contract Shape { area(): real64; } "
                                   "record Circle with Shape { static fn area(): real64 { return 1.0; } }",
                                   "record 'Circle' declares contract 'Shape' but is missing method 'area(): real64'",
                                   "expected static method not to satisfy contract") &&
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
    passed = expect_error_contains("import math; io.println(math.UNKNOWN);", "module 'math' does not export 'UNKNOWN'",
                                   "expected missing module value") &&
             passed;
    passed = expect_error_contains("fn choose(value: i64): i64 { return value; } "
                                   "fn choose(value: u64): u64 { return value; } io.println(choose(1));",
                                   "ambiguous overload for function 'choose'", "expected ambiguous overload") &&
             passed;
    passed = expect_error_contains("fn same(value: int): int { return value; } "
                                   "fn same(value: int): int { return value; }",
                                   "duplicate overload for function 'same'", "expected duplicate overload") &&
             passed;
    passed = expect_error_contains("import time;", "unknown module 'time'", "expected unknown module error") && passed;

    passed = expect_valid("nums: [int] = [1, 2, 3]; squares: [int] = [n * n for n in nums]; "
                          "first: int = squares[0]; io.println(first);",
                          "expected array comprehension to validate") &&
             passed;
    passed = expect_valid("evens: [int] = [i for i in 0..10 if i % 2 == 0]; io.println(evens.len());",
                          "expected range comprehension with filter to validate") &&
             passed;
    passed = expect_valid("import io; import set; seen: set.Set = set.Set.new(); seen.add(\"a\"); "
                          "values: [text] = [value for value in seen if value.len() > 0]; io.println(values.len());",
                          "expected set comprehension to validate") &&
             passed;
    passed = expect_error_contains("nums: [int] = [1, 2, 3]; bad: [int] = [n for n in nums if n + 1];",
                                   "expected type 'bool'", "expected non-boolean comprehension filter error") &&
             passed;
    passed = expect_error_contains("bad: [int] = [n for n in 42];", "is not iterable",
                                   "expected non-iterable comprehension error") &&
             passed;
    passed = expect_error_contains("nums: [int] = [1, 2, 3]; bad: [text] = [n for n in nums];", "expected type 'text'",
                                   "expected comprehension element type mismatch error") &&
             passed;
    passed = expect_error_contains("nums: [int] = [1, 2, 3]; xs: [int] = [n for n in nums]; io.println(n);",
                                   "undefined variable 'n'", "expected comprehension variable to be scoped") &&
             passed;

    passed = expect_valid("import outcome; "
                          "fn pipe(): outcome.Outcome<int, text> { "
                          "value: int = outcome.done_int(41)? + 1; return outcome.done_int(value); }",
                          "expected '?' on Outcome to validate") &&
             passed;
    passed =
        expect_error_contains("fn f(): int { value: int = 5; return value?; }",
                              "'?' can only be applied to an 'outcome.Outcome'", "expected '?' on non-Outcome error") &&
        passed;
    passed = expect_error_contains("import outcome; fn f(): int { return outcome.done_int(1)?; }",
                                   "'?' requires the enclosing function to return 'outcome.Outcome'",
                                   "expected '?' in non-Outcome function error") &&
             passed;
    passed =
        expect_error_contains("import outcome; fn f(): outcome.Outcome<int, int> { return outcome.done_int(1)?; }",
                              "does not match the function error type", "expected '?' error-type mismatch error") &&
        passed;
    passed = expect_error_contains("import outcome; value: int = outcome.done_int(1)?;",
                                   "'?' can only be used inside a function", "expected '?' at top-level error") &&
             passed;

    // --- Foreknown compile-time declarations --------------------------------
    passed = expect_valid("foreknown const KB: int = 1024; "
                          "foreknown fn scale(n: int): int { return n * KB; } "
                          "foreknown const SIZE: int = scale(4); io.println(SIZE);",
                          "expected foreknown constants and function to type check") &&
             passed;
    passed =
        expect_error_contains("fn runtime_value(): int { return 1; } "
                              "foreknown const BAD: int = runtime_value();",
                              "function 'runtime_value' is not foreknown", "expected non-foreknown call diagnostic") &&
        passed;
    passed = expect_error_contains("fn side_effect(): int { return 1; } "
                                   "foreknown fn bad(): int { return side_effect(); }",
                                   "is not foreknown", "expected foreknown body diagnostic") &&
             passed;
    passed = expect_error_contains("foreknown fn identity<T>(value: T): T { return value; }",
                                   "generic foreknown functions are not supported yet",
                                   "expected generic foreknown diagnostic") &&
             passed;

    passed = expect_valid("import fs; import process; import csv; "
                          "r = fs.read_text(\"a.txt\"); io.println(r.is_done()); "
                          "io.println(process.env_or(\"HOME\", \"none\")); "
                          "io.println(process.arg_count()); "
                          "io.println(csv.parse_rows(\"a,b\").len());",
                          "expected fs/process/csv modules to type check") &&
             passed;
    passed = expect_valid("import io; import plot; import text; "
                          "chart = plot.line([1.0, 2.0], [3.0, 4.0]).title(\"demo\").x_label(\"x\"); "
                          "io.println(plot.svg(chart).contains(\"<svg\")); "
                          "plot.use_backend(\"svg\"); io.println(plot.backend()); io.println(plot.show(chart).is_done()); "
                          "native_result = plot.show_native(chart); io.println(native_result.is_done());",
                          "expected plot module to type check") &&
             passed;
    passed = expect_valid("import io; import canvas; import text; "
                          "scene: canvas.Canvas = canvas.new(\"demo\", 320, 200); "
                          "scene = scene.grid(40.0, \"#94a3b8\"); "
                          "scene = scene.button(16.0, 16.0, 80.0, 28.0, \"Run\", true); "
                          "scene = scene.checkbox(16.0, 58.0, \"snap\", false); "
                          "scene = scene.slider(16.0, 104.0, 120.0, 0.0, 10.0, 4.0, \"zoom\"); "
                          "scene = scene.table(160.0, 16.0, [54.0, 54.0], [\"tool\", \"key\"], [[\"grid\", \"g\"]]); "
                          "io.println(canvas.svg(scene).contains(\"<svg\")); "
                          "native_result = scene.show_native(); io.println(native_result.is_done());",
                          "expected canvas module to type check") &&
             passed;
    passed = expect_error_contains("x = __read_file(1);", "expected type 'text' but got 'int'",
                                   "expected __read_file argument type error") &&
             passed;
    passed = expect_error_contains("x = __write_file(\"a.txt\");", "__write_file expects 2 arguments but got 1",
                                   "expected __write_file arity error") &&
             passed;
    passed = expect_error_contains("x = __plot_backend_get(\"svg\");", "__plot_backend_get expects 0 arguments",
                                   "expected __plot_backend_get arity error") &&
             passed;
    passed = expect_error_contains("__plot_backend_set(1);", "expected type 'text' but got 'int'",
                                   "expected __plot_backend_set argument type error") &&
             passed;
    passed = expect_error_contains("x = __plot_show_native();", "__plot_show_native expects 1 arguments",
                                   "expected __plot_show_native arity error") &&
             passed;
    passed = expect_error_contains("__plot_show_native(1);", "expected type 'text' but got 'int'",
                                   "expected __plot_show_native argument type error") &&
             passed;
    passed = expect_error_contains("x = __canvas_show_native(\"demo\");", "__canvas_show_native expects 2 arguments",
                                   "expected __canvas_show_native arity error") &&
             passed;
    passed = expect_error_contains("__canvas_show_native(1, \"<svg/>\");", "expected type 'text' but got 'int'",
                                   "expected __canvas_show_native argument type error") &&
             passed;

    passed = expect_valid("record Point { x: int, fn to_text(): text { return fmt.format(\"{}\", this.x); } } "
                          "p: Point = Point { x: 1 }; io.println(p); io.println(fmt.format(\"{}\", p)); "
                          "label: text = fmt.format(\"{}\", p);",
                          "expected record with to_text to be printable") &&
             passed;
    passed = expect_error_contains("record Bare { x: int } b: Bare = Bare { x: 1 }; io.println(b);",
                                   "add a 'to_text(): text' method", "expected non-printable record error") &&
             passed;

    // --- First-class function values and method chaining --------------------
    passed = expect_valid("import array; "
                          "fn square(x: int): int { return x * x; } "
                          "fn is_positive(x: int): bool { return x > 0; } "
                          "values = [0 - 1, 2, 3]; "
                          "total: int = values.filter(is_positive).map(square).sum(); io.println(total);",
                          "expected filter/map/sum chain to type check") &&
             passed;
    passed = expect_valid("fn add(acc: int, x: int): int { return acc + x; } "
                          "method<T, Acc> [T].fold_left(initial: Acc, combine: fn(Acc, T): Acc): Acc { "
                          "result: Acc = initial; "
                          "for i = 0; i < this.len(); i = i + 1 { result = combine(result, this[i]); } "
                          "return result; } "
                          "sum: int = [1, 2, 3].fold_left(0, add); io.println(sum);",
                          "expected user-defined higher-order method to type check") &&
             passed;
    // The callback signature must match the element type: `square` is fn(int): int
    // but a [bool] receiver needs fn(bool): U, so the diagnostic names both.
    passed = expect_error_contains("import array; fn square(x: int): int { return x * x; } "
                                   "flags = [true, false]; bad = flags.map(square);",
                                   "has no method 'map' matching argument types (fn(int): int)",
                                   "expected mismatched-callback diagnostic") &&
             passed;
    // Passing a non-function where a callback is expected is reported too.
    passed = expect_error_contains("import array; values = [1, 2, 3]; bad = values.filter(7);",
                                   "has no method 'filter' matching argument types (int)",
                                   "expected non-function callback diagnostic") &&
             passed;
    // Calling a function value with the wrong arity is a clear error.
    passed = expect_error_contains("fn takes_two(a: int, b: int): int { return a + b; } "
                                   "method<T> [T].run_on_first(f: fn(T): T): T { return f(this[0]); } "
                                   "x: int = [1, 2].run_on_first(takes_two);",
                                   "has no method 'run_on_first' matching argument types (fn(int, int): int)",
                                   "expected callback-arity diagnostic") &&
             passed;
    // A generic function cannot be captured as a plain value without a call site.
    passed = expect_error_contains("fn identity<T>(value: T): T { return value; } "
                                   "grab = identity;",
                                   "generic function 'identity' cannot be used as a value",
                                   "expected generic-function-as-value diagnostic") &&
             passed;

    // Lambdas and closures: contextual parameter typing, nested returned
    // closures, generic captures, aggregate handles, and callable expressions.
    passed = expect_valid("factor: int = 10; "
                          "scale: fn(int): int = fn(value) { value * factor }; "
                          "make_adder = fn(base: int): fn(int): int { fn(value: int): int { base + value } }; "
                          "add_two = make_adder(2); direct: int = make_adder(3)(4); "
                          "immediate: int = (fn(value: int): int { value + 1 })(41); "
                          "items = [1]; append = fn(value: int): unit { items.push(value); return; }; "
                          "append(2); io.println(scale(add_two(direct + immediate)));",
                          "expected typed lambdas and closure captures to validate") &&
             passed;
    passed = expect_valid("factory: fn(text): fn(text): text = fn(prefix) { fn(suffix) { prefix + suffix } }; "
                          "joined: text = factory(\"du\")(\"ne\");",
                          "expected nested lambda tail expressions to inherit contextual function types") &&
             passed;
    passed = expect_valid("fn remember<T>(value: T): fn(): T { fn(): T { value } } "
                          "get_int = remember(42); get_text = remember(\"dune\"); "
                          "answer: int = get_int(); label: text = get_text();",
                          "expected closures in generic functions to monomorphize") &&
             passed;
    passed = expect_valid("record Box { value: int } box = Box { value: 1 }; "
                          "update = fn(next: int): unit { box.value = next; return; }; update(2);",
                          "expected captured record handles to remain mutable") &&
             passed;
    passed = expect_error_contains("value: int = 1; change = fn(): int { value = 2; value };",
                                   "cannot reassign captured variable 'value'",
                                   "expected captured binding reassignment diagnostic") &&
             passed;
    passed = expect_error_contains("left = 1; right = 2; change = fn(): unit { (left, right) = (3, 4); return; };",
                                   "cannot reassign captured variable 'left'",
                                   "expected captured tuple binding reassignment diagnostic") &&
             passed;
    passed = expect_error_contains("bad: fn(int): bool = fn(value: int): int { value };",
                                   "expected type 'fn(int): bool' but got 'fn(int): int'",
                                   "expected lambda signature mismatch diagnostic") &&
             passed;
    passed = expect_error_contains("f = fn(value: int): int { value }; result = f(true);",
                                   "expected type 'int' but got 'bool'", "expected lambda argument diagnostic") &&
             passed;
    passed =
        expect_error_contains("f = fn(value: int): int { value }; result = f();",
                              "function value expects 1 argument(s) but got 0", "expected lambda arity diagnostic") &&
        passed;
    passed = expect_error_contains("value = 42; result = value();", "cannot call value of type 'int'",
                                   "expected non-callable value diagnostic") &&
             passed;
    passed = expect_error_contains("for i = 0; i < 1; i = i + 1 { f = fn(): int { break; 1 }; }",
                                   "break statement outside loop",
                                   "expected lambda loop control not to escape into outer loop") &&
             passed;

    // --- Modules v2: aliases, selective / grouped imports, visibility ---------
    passed = expect_valid("import math as m; from array import range, sum; "
                          "total: int = sum(range(1, 4)); ok: bool = m.PI > 3.0; io.println(total); io.println(ok);",
                          "expected alias + selective/grouped stdlib imports to type check") &&
             passed;
    passed = expect_fixture_valid("module app; import shapes as sh; from shapes import Point, manhattan; "
                                  "a = Point { x: 1, y: 2 }; b = sh.Point { x: 4, y: 6 }; "
                                  "d: int = manhattan(a, b); io.println(d); io.println(sh.quadrant_sum(a));",
                                  "expected local module with alias + selective import to type check") &&
             passed;
    passed = expect_error_contains("import math as m; import array as m; io.println(m.PI);",
                                   "import alias 'm' is already bound to module 'math'",
                                   "expected duplicate-alias diagnostic") &&
             passed;
    passed = expect_error_contains("import array; import math as array; io.println(1);",
                                   "import alias 'array' conflicts with imported module 'array'",
                                   "expected alias-vs-module diagnostic") &&
             passed;
    passed = expect_error_contains("from math import definitely_missing; io.println(1);",
                                   "module 'math' does not export 'definitely_missing'",
                                   "expected unknown-exported-symbol diagnostic") &&
             passed;
    // A private (non-exported) symbol cannot be selectively imported.
    passed = expect_fixture_error_contains("from feature_exports import hidden; io.println(hidden());",
                                           "module 'feature_exports' does not export 'hidden'",
                                           "expected private-symbol import diagnostic") &&
             passed;
    // Non-exported members stay invisible even when the module is imported plainly.
    passed = expect_fixture_error_contains("import feature_exports; io.println(feature_exports.hidden());",
                                           "module 'feature_exports' does not export 'hidden'",
                                           "expected private-member access diagnostic") &&
             passed;

    // A top-level test block sees globals and functions; its body is a normal block.
    passed = expect_valid("import io; const LIMIT: int = 3; fn twice(x: int): int { return x + x; } "
                          "test \"uses globals\" { total: int = twice(LIMIT); io.println(total); }",
                          "expected a top-level test block to type-check") &&
             passed;
    // Test blocks are only allowed at the top level.
    passed = expect_error_contains("fn f(): unit { test \"nested\" { } }",
                                   "test blocks are only allowed at top level",
                                   "expected a nested test block to be rejected") &&
             passed;

    // --- Const generics and static shapes for Matrix/Vector (issue #43) --------
    // Statically-shaped annotations parse and type-check, and a shape-compatible
    // matrix-vector product flows its result shape into a matching binding.
    passed = expect_valid("import matrix; "
                          "a: matrix.Matrix<real64, 3, 3> = matrix.identity(3); "
                          "v: matrix.Vector<real64, 3> = matrix.vector([1.0, 2.0, 3.0]); "
                          "r: matrix.Vector<real64, 3> = a.mul_vector(v); io.println(r.len());",
                          "expected compatible static matrix-vector product to validate") &&
             passed;
    // A shape-compatible matrix product (2x3 · 3x2 → 2x2) type-checks.
    passed = expect_valid("import matrix; "
                          "a: matrix.Matrix<real64, 2, 3> = matrix.from_rows([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]); "
                          "b: matrix.Matrix<real64, 3, 2> = matrix.from_rows([[7.0, 8.0], [9.0, 10.0], [11.0, 12.0]]); "
                          "p: matrix.Matrix<real64, 2, 2> = a.matmul(b); io.println(p.rows());",
                          "expected compatible static matrix product to validate") &&
             passed;
    // Same-shape element-wise addition keeps the shape.
    passed = expect_valid("import matrix; "
                          "a: matrix.Matrix<int, 2, 2> = matrix.from_rows([[1, 2], [3, 4]]); "
                          "b: matrix.Matrix<int, 2, 2> = matrix.from_rows([[5, 6], [7, 8]]); "
                          "c: matrix.Matrix<int, 2, 2> = a.add(b); io.println(c.rows());",
                          "expected same-shape matrix addition to validate") &&
             passed;
    // The dynamic API is untouched: unannotated matrix code keeps working.
    passed = expect_valid("import matrix; "
                          "a = matrix.from_rows([[1, 2, 3], [4, 5, 6]]); "
                          "b = matrix.from_rows([[7, 8], [9, 10], [11, 12]]); "
                          "p = matrix.dot(a, b); io.println(p.rows());",
                          "expected dynamic matrix API to keep validating") &&
             passed;
    // Static and dynamic shapes coexist: a static value is assignable to a dynamic
    // binding and vice versa.
    passed = expect_valid("import matrix; "
                          "a: matrix.Matrix<real64, 2, 2> = matrix.identity(2); "
                          "dyn: matrix.Matrix<real64> = a; "
                          "back: matrix.Matrix<real64, 2, 2> = dyn; io.println(back.rows());",
                          "expected static and dynamic matrix shapes to coexist") &&
             passed;
    // A statically-shaped vector still iterates over its element type.
    passed = expect_valid("import matrix; "
                          "v: matrix.Vector<int, 3> = matrix.vector([1, 2, 3]); "
                          "last: int = 0; for x in v { last = x; } io.println(last);",
                          "expected a static vector to remain iterable") &&
             passed;
    // A matrix-vector product whose inner dimensions disagree is a compile error.
    passed = expect_error_contains("import matrix; "
                                   "a: matrix.Matrix<real64, 3, 3> = matrix.identity(3); "
                                   "v: matrix.Vector<real64, 4> = matrix.vector([1.0, 2.0, 3.0, 4.0]); "
                                   "bad = a.mul_vector(v);",
                                   "matrix-vector shape mismatch",
                                   "expected incompatible matrix-vector product to be rejected") &&
             passed;
    // Binding a shaped result to the wrong static shape is rejected with expected/actual.
    passed = expect_error_contains("import matrix; "
                                   "a: matrix.Matrix<real64, 3, 3> = matrix.identity(3); "
                                   "v: matrix.Vector<real64, 3> = matrix.vector([1.0, 2.0, 3.0]); "
                                   "bad: matrix.Vector<real64, 4> = a.mul_vector(v);",
                                   "matrix.Vector<real, 4>",
                                   "expected a wrong-shape binding to be rejected") &&
             passed;
    // Multiplying matrices whose inner dimensions disagree is a compile error.
    passed = expect_error_contains("import matrix; "
                                   "a: matrix.Matrix<real64, 2, 3> = matrix.from_rows([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]); "
                                   "b: matrix.Matrix<real64, 2, 2> = matrix.from_rows([[7.0, 8.0], [9.0, 10.0]]); "
                                   "bad = a.matmul(b);",
                                   "matrix shape mismatch",
                                   "expected incompatible matrix product to be rejected") &&
             passed;
    // A dot product between vectors of different lengths is a compile error.
    passed = expect_error_contains("import matrix; "
                                   "u: matrix.Vector<real64, 3> = matrix.vector([1.0, 2.0, 3.0]); "
                                   "w: matrix.Vector<real64, 4> = matrix.vector([1.0, 2.0, 3.0, 4.0]); "
                                   "bad = u.dot(w);",
                                   "vector length mismatch",
                                   "expected a mismatched-length dot product to be rejected") &&
             passed;
    // Element-wise addition of different-shape matrices is a compile error.
    passed = expect_error_contains("import matrix; "
                                   "a: matrix.Matrix<int, 2, 2> = matrix.from_rows([[1, 2], [3, 4]]); "
                                   "b: matrix.Matrix<int, 3, 3> = matrix.identity(3); "
                                   "bad = a.add(b);",
                                   "matrix shape mismatch",
                                   "expected different-shape matrix addition to be rejected") &&
             passed;
    // A Matrix with the wrong number of static dimensions is rejected.
    passed = expect_error_contains("import matrix; a: matrix.Matrix<real64, 3> = matrix.identity(3);",
                                   "Matrix takes either no static shape or exactly 2 dimensions",
                                   "expected a mis-shaped Matrix annotation to be rejected") &&
             passed;
    // A Vector with more than one static dimension is rejected.
    passed = expect_error_contains("import matrix; v: matrix.Vector<int, 3, 3> = matrix.vector([1, 2, 3]);",
                                   "Vector takes either no static shape or exactly 1 dimension",
                                   "expected a mis-shaped Vector annotation to be rejected") &&
             passed;
    // Phase 1 accepts positive integer literals only: a negative dimension is rejected.
    passed = expect_error_contains("import matrix; v: matrix.Vector<int, -3> = matrix.vector([1, 2, 3]);",
                                   "const generic argument must be a positive integer literal",
                                   "expected a negative const generic argument to be rejected") &&
             passed;

    return passed ? 0 : 1;
}
