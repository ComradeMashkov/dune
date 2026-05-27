#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "vm/vm.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string run_source(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::Compiler compiler;
    dune::VirtualMachine vm(compiler.compile(loader.resolve(parser.parse())));

    std::ostringstream output;
    vm.run(output);
    return output.str();
}

bool expect_eq(const std::string& actual, const std::string& expected, const char* message) {
    if (actual != expected) {
        std::cerr << message << ": expected '" << expected << "', got '" << actual << "'\n";
        return false;
    }

    return true;
}

bool expect_throws(const std::string& source, const char* message) {
    try {
        run_source(source);
    } catch (const std::runtime_error&) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

bool expect_error_contains(const std::string& source, const std::string& expected, const char* message) {
    try {
        run_source(source);
    } catch (const std::runtime_error& error) {
        const std::string actual = error.what();
        if (actual.find(expected) == std::string::npos) {
            std::cerr << message << ": expected error containing '" << expected << "', got '" << actual << "'\n";
            return false;
        }

        return true;
    }

    std::cerr << message << '\n';
    return false;
}

} // namespace

int main() {
    bool passed = true;

    passed = expect_eq(run_source("print(40 + 2);"), "42\n", "expected arithmetic output") && passed;
    passed = expect_eq(run_source("print(true); print(false); print(3 > 2); print(3 != 3);"), "1\n0\n1\n0\n",
                       "expected boolean and comparison output") &&
             passed;
    passed =
        expect_eq(run_source("x := 10; y := x * 3 - 4 / 2; print(y);"), "28\n", "expected variable output") && passed;
    passed = expect_eq(run_source("x := 1; x = x + 2; print(x);"), "3\n", "expected reassignment") && passed;
    passed = expect_eq(run_source("x := 3; while x > 0 { x = x - 1; } "
                                  "if x == 0 { print(42); } else { print(0); }"),
                       "42\n", "expected control flow output") &&
             passed;
    passed = expect_eq(run_source("add(a, b) { return a + b; } print(add(10, 20));"), "30\n",
                       "expected function call output") &&
             passed;
    passed = expect_eq(run_source("add(a: int, b: int): int { return a + b; } "
                                  "twice(value: int): int { return add(value, value); } "
                                  "print(add(twice(5), add(3, 4)));"),
                       "17\n", "expected nested function call output") &&
             passed;
    passed = expect_eq(run_source("choose(flag: bool, yes: int, no: int): int { "
                                  "if flag { return yes; } else { return no; } } "
                                  "print(choose(false, 1, 2));"),
                       "2\n", "expected function return through branches") &&
             passed;
    passed = expect_eq(run_source("show(value: int): int { return value + 1; } "
                                  "show(value: bool): int { if value { return 10; } else { return 20; } } "
                                  "print(show(41)); print(show(false));"),
                       "42\n20\n", "expected overloaded function dispatch") &&
             passed;
    passed = expect_eq(run_source("small: u8 := 250; wide: uint64 := 10000000000; "
                                  "total: uint64 := wide + 5; print(small); print(total);"),
                       "250\n10000000005\n", "expected unsigned output") &&
             passed;
    passed = expect_eq(run_source("ratio: real := 1 + 2.5; print(ratio / 2.0);"), "1.75\n", "expected real output") &&
             passed;
    passed = expect_eq(run_source("mark: glyph := 'Z'; print(mark);"), "Z\n", "expected glyph output") && passed;
    passed = expect_eq(run_source("log(message: text): unit { print(message); return; } "
                                  "noop(): unit { } "
                                  "tiny: i8 := 127; small: i16 := 32767; mid: i32 := 2147483647; "
                                  "wide: i64 := 9000000000; index: usize := 5; offset: isize := 6; "
                                  "rough: real32 := 1 + 2.5; exact: real64 := 2.25; "
                                  "log(\"types\"); noop(); print(tiny); print(small); print(mid); "
                                  "print(wide); print(index); print(offset); print(rough); print(exact); "
                                  "if \"same\" == \"same\" { print(\"same\"); } else { print(\"bad\"); }"),
                       "types\n127\n32767\n2147483647\n9000000000\n5\n6\n3.5\n2.25\nsame\n",
                       "expected standard type output") &&
             passed;
    passed = expect_eq(run_source("import math; "
                                  "values: [int] := [1, math.square(2), 5]; "
                                  "base: u64 := 7; "
                                  "values.push(math.square(values[2])); "
                                  "print(values.len()); print(values[1]); print(values[3]); "
                                  "print(math.square(base)); print(math.square(1.5)); "
                                  "print(math.abs(0 - 8)); print(math.cube(3)); print(math.min(4, 9)); "
                                  "print(math.max(4, 9)); print(math.clamp(12, 0, 10)); "
                                  "rough: real32 := 1.5; print(math.max(rough, 2.5)); "
                                  "print(math.abs(0.0 - 2.5)); print(math.round(math.PI)); "
                                  "print(math.floor(2.9)); print(math.ceil(2.1)); print(math.sqrt(9.0)); "
                                  "print(math.pow(2.0, 3)); print(math.sin(0.0)); print(math.cos(0.0)); "
                                  "print(math.tan(0.0)); print(math.exp(0.0)); print(math.ln(1.0)); "
                                  "print(math.normalize_radians(math.TAU));"),
                       "4\n4\n25\n49\n2.25\n8\n27\n4\n9\n10\n2.5\n2.5\n3\n2\n3\n3\n8\n0\n1\n0\n1\n0\n0\n",
                       "expected arrays and module output") &&
             passed;
    passed = expect_eq(run_source("value := 17; print(0 - value); print(-value); print(17 % 5); "
                                  "print(!false); print(false && (1 / 0 == 0)); print(true || (1 / 0 == 0)); "
                                  "count: real64 := value to real64; print(count / 2.0); "
                                  "code: int := 'A' to int; letter: glyph := 66 to glyph; "
                                  "flag: bool := 0 to bool; print(code); print(letter); print(flag); "
                                  "values: [int] := [1, 2]; print(values.is_empty()); values.push(3); "
                                  "print(values.pop()); print(values.len()); values.clear(); print(values.is_empty()); "
                                  "message: text := \"dune language\"; print(message.len()); "
                                  "print(message.contains(\"lang\")); print(message.starts_with(\"dune\")); "
                                  "print(\"\".is_empty());"),
                       "-17\n-17\n2\n1\n0\n1\n8.5\n65\nB\n0\n0\n3\n2\n1\n13\n1\n1\n1\n",
                       "expected operators casts and methods output") &&
             passed;
    passed = expect_eq(run_source("foreign c_sqrt(value: real64): real64 = \"sqrt\"; "
                                  "message: text := \"dune language\"; print(message[0]); "
                                  "print(message[5:13]); print(message[:4]); print(message[5:]); "
                                  "values: [int] := [1, 2, 3, 4, 5]; middle: [int] := values[1:4]; "
                                  "print(middle.len()); print(middle[0]); print(middle[2]); "
                                  "total := 0; "
                                  "for i := 0; i < 6; i = i + 1 { "
                                  "if i == 1 { continue; } if i == 4 { break; } total = total + i; } "
                                  "print(total); print(c_sqrt(81.0));"),
                       "d\nlanguage\ndune\nlanguage\n3\n2\n4\n5\n9\n",
                       "expected foreign functions slices text indexing and for loop output") &&
             passed;
    passed = expect_eq(run_source("import array; import text; "
                                  "values: [int] := [1, 2, 3]; reversed: [int] := array.reverse(values); "
                                  "print(array.sum(reversed)); print(array.first(reversed)); "
                                  "print(array.last(reversed)); print(array.contains(values, 2)); "
                                  "print(array.index_of(values, 3)); "
                                  "print(values.first()); print(values.append(4).last()); "
                                  "ranged: [int] := array.range(2, 5); print(array.sum(ranged)); "
                                  "message: text := \" dune language \"; print(text.trim(message)); "
                                  "print(text.ends_with(text.trim(message), \"age\")); "
                                  "print(message.trim().ends_with(\"age\")); "
                                  "print(text.index_of(message, 'l')); print(text.count(message, 'a')); "
                                  "print(text.is_digit('7')); print(text.is_alpha('Z'));"),
                       "6\n3\n1\n1\n2\n1\n4\n9\ndune language\n1\n1\n6\n2\n1\n1\n",
                       "expected array and text stdlib module output") &&
             passed;
    passed = expect_eq(run_source("import array; import math; "
                                  "identity<T>(value: T): T { return value; } "
                                  "twice<T is numeric>(value: T): T { return value + value; } "
                                  "words: [text] := [\"dune\", \"lang\"]; "
                                  "reversed: [text] := array.reverse(words); "
                                  "rough: real32 := 1.5; "
                                  "print(identity(42)); print(identity(\"done\")); print(reversed.first()); "
                                  "print(math.square(12)); print(math.square(rough)); print(twice(9));"),
                       "42\ndone\nlang\n144\n2.25\n18\n", "expected generic functions and generic stdlib output") &&
             passed;
    passed = expect_eq(run_source("// comments are ignored\n"
                                  "import array; // stdlib receiver methods\n"
                                  "values: [int] := [7, 8];\n"
                                  "print(values[0]); // arrays are zero-based\n"
                                  "print(values.first());\n"
                                  "print(8 / 2);"),
                       "7\n7\n4\n", "expected comments and zero-based array output") &&
             passed;
    passed = expect_eq(run_source("record Point { x: int, y: int, "
                                  "sum(): int { return this.x + this.y; } } "
                                  "make(x: int, y: int): Point { return Point { x: x, y: y }; } "
                                  "p: Point := make(10, 20); print(p.x); print(p.y); print(p.sum());"),
                       "10\n20\n30\n", "expected record fields and methods output") &&
             passed;
    passed = expect_eq(run_source("record Box<T> { value: T, value_or(default: T): T { return this.value; } } "
                                  "boxed<T>(value: T): Box<T> { return Box { value: value }; } "
                                  "number: Box<int> := boxed(7); label: Box<text> := boxed(\"done\"); "
                                  "print(number.value_or(0)); print(label.value_or(\"bad\")); "
                                  "print(when number.value { is 7 { 70 } is _ { 0 } }); "
                                  "print(when \"dune\" { is \"lang\" { 1 } is _ { 2 } });"),
                       "7\ndone\n70\n2\n", "expected generic records and when output") &&
             passed;
    passed = expect_eq(run_source("choice Maybe { Present(int), Absent, } "
                                  "x := 99; value: Maybe := Present(30); missing: Maybe := Absent; "
                                  "print(when value { is Present(x) { x + 1 } is Absent { 0 } }); "
                                  "print(when missing { is Present(x) { x } is Absent { 7 } }); print(x);"),
                       "31\n7\n99\n", "expected choice variants and scoped when bindings") &&
             passed;
    passed = expect_eq(run_source("import maybe; import outcome; import assert; import collections; "
                                  "maybe_value: maybe.Maybe<int> := maybe.present(42); "
                                  "missing: maybe.Maybe<int> := maybe.absent(0); "
                                  "failed: outcome.Outcome<int, text> := outcome.failed(0, \"bad\"); "
                                  "repeated: [int] := collections.repeat_int(3, 4); "
                                  "print(maybe_value.value_or(0)); print(missing.value_or(7)); "
                                  "print(failed.failure_or(\"absent\")); print(repeated.len()); "
                                  "print(assert.equals_int(repeated[0], 3));"),
                       "42\n7\nbad\n4\n1\n", "expected record stdlib module output") &&
             passed;
    passed = expect_throws("print(missing);", "expected undefined variable to throw") && passed;
    passed = expect_throws("missing = 1;", "expected undefined assignment to throw") && passed;
    passed = expect_throws("print(1 / 0);", "expected division by zero to throw") && passed;
    passed = expect_error_contains("values: [int] := [1]; print(values[2]);", "array index out of bounds",
                                   "expected array bounds error") &&
             passed;
    passed = expect_error_contains("message: text := \"done\"; print(message[4]);", "text index out of bounds",
                                   "expected text bounds error") &&
             passed;
    passed =
        expect_error_contains("values: [int] := [1, 2]; bad: [int] := values[2:1];",
                              "slice start cannot be greater than slice end", "expected invalid slice range error") &&
        passed;
    passed =
        expect_error_contains("x: int := true;", "expected type 'int' but got 'bool'", "expected static type error") &&
        passed;

    return passed ? 0 : 1;
}
