#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
#include "modules/module_loader.hpp"
#include "parser/parser.hpp"
#include "vm/vm.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

std::string test_source(const std::string& source) {
    return "import io; import fmt; " + source;
}

std::string run_source(const std::string& source) {
    dune::Lexer lexer(test_source(source));
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::Compiler compiler;
    dune::VirtualMachine vm(compiler.compile(loader.resolve(parser.parse())));

    std::ostringstream output;
    vm.run(output);
    return output.str();
}

std::string run_source_with_args(const std::string& source, std::vector<std::string> arguments) {
    dune::Lexer lexer(test_source(source));
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::Compiler compiler;
    dune::VirtualMachine vm(compiler.compile(loader.resolve(parser.parse())), std::move(arguments));

    std::ostringstream output;
    vm.run(output);
    return output.str();
}

// Compiles `source`, then runs only its `test` blocks through `run_test` — the
// same path `dune test` takes — and returns their concatenated output. Top-level
// code is never executed, mirroring the runner.
std::string run_only_tests(const std::string& source) {
    dune::Lexer lexer(source);
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::Compiler compiler;
    dune::Bytecode bytecode = compiler.compile(loader.resolve(parser.parse()));
    const std::vector<dune::Bytecode::Test> tests(bytecode.tests);
    dune::VirtualMachine vm(std::move(bytecode));

    std::ostringstream output;
    for (const dune::Bytecode::Test& test : tests) {
        vm.run_test(test.function_index, output);
    }

    return output.str();
}

struct RunResult {
    std::string output;
    std::string error;
};

RunResult run_source_with_streams(const std::string& source, const std::string& input_text) {
    dune::Lexer lexer(test_source(source));
    dune::Parser parser(lexer.tokenize());
    dune::ModuleLoader loader;
    dune::Compiler compiler;
    dune::VirtualMachine vm(compiler.compile(loader.resolve(parser.parse())));

    std::istringstream input(input_text);
    std::ostringstream output;
    std::ostringstream error;
    vm.run(output, error, input);
    return RunResult{output.str(), error.str()};
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

bool runs_process_stdlib() {
    bool passed = true;
    passed = expect_eq(run_source("import process; io.println(process.env_or(\"DUNE_UNSET_VAR_XYZ_123\", \"fallback\"));"),
                       "fallback\n", "expected process.env_or default for unset variable") &&
             passed;
    passed = expect_eq(run_source_with_args("import process; io.println(process.arg_count()); "
                                            "for a in process.args() { io.println(a); } "
                                            "io.println(process.arg(1).value_or(\"none\")); "
                                            "io.println(process.arg(9).value_or(\"none\"));",
                                            {"alpha", "beta"}),
                       "2\nalpha\nbeta\nbeta\nnone\n", "expected process args API") &&
             passed;
    passed = expect_eq(run_source("import process; io.println(process.cwd().has_value());"), "1\n",
                       "expected process.cwd to be present") &&
             passed;
    return passed;
}

bool runs_io_stdlib() {
    bool passed = true;

    const RunResult output_result = run_source_with_streams("import io; "
                                                           "io.print(\"hello\"); io.println(\" world\"); "
                                                           "io.eprint(\"warn\"); io.eprintln(\"ing\"); "
                                                           "io.println(io.flush().is_done()); "
                                                           "io.println(io.flush_err().is_done());",
                                                           "");
    passed = expect_eq(output_result.output, "hello world\n1\n1\n", "expected io stdout output") && passed;
    passed = expect_eq(output_result.error, "warning\n", "expected io stderr output") && passed;

    const RunResult input_result = run_source_with_streams("import io; "
                                                          "first = io.read_line(); "
                                                          "second = io.read_line(); "
                                                          "missing = io.read_line(); "
                                                          "io.println(first.value_or(\"<none>\")); "
                                                          "io.println(second.value_or(\"<none>\")); "
                                                          "io.println(missing.is_failed()); "
                                                          "io.println(missing.failure_or(\"ok\"));",
                                                          "alpha\nbeta\n");
    passed = expect_eq(input_result.output, "alpha\nbeta\n1\nend of input\n", "expected io read_line output") &&
             passed;
    passed = expect_eq(input_result.error, "", "expected no io read_line stderr") && passed;

    const RunResult prompt_result = run_source_with_streams("import io; "
                                                           "name = io.prompt(\"name: \"); "
                                                           "io.println(name.value_or(\"<none>\"));",
                                                           "Ada\n");
    passed = expect_eq(prompt_result.output, "name: Ada\n", "expected io prompt output") && passed;
    passed = expect_eq(prompt_result.error, "", "expected no io prompt stderr") && passed;

    return passed;
}

bool runs_fs_stdlib() {
    std::error_code error;
    const std::filesystem::path file = std::filesystem::temp_directory_path() / "dune_vm_fs_test.txt";
    const std::filesystem::path missing = std::filesystem::temp_directory_path() / "dune_vm_fs_missing_xyz.txt";
    std::filesystem::remove(file, error);
    std::filesystem::remove(missing, error);

    const std::string source = "import fs; "
                               "w = fs.write_text(\"" +
                               file.generic_string() +
                               "\", \"io-content\"); "
                               "io.println(w.is_done()); "
                               "r = fs.read_text(\"" +
                               file.generic_string() +
                               "\"); "
                               "io.println(r.value_or(\"<none>\")); "
                               "m = fs.read_text(\"" +
                               missing.generic_string() +
                               "\"); "
                               "io.println(m.is_failed());";

    const bool passed = expect_eq(run_source(source), "1\nio-content\n1\n", "expected fs write/read/missing output");
    std::filesystem::remove(file, error);
    return passed;
}

bool runs_csv_stdlib() {
    return expect_eq(run_source("import csv; "
                                "rows = csv.parse_rows(\"a,b,c\\nd,e,f\"); "
                                "io.println(rows.len()); io.println(rows[0][1]); io.println(rows[1][2]);"),
                     "2\nb\nf\n", "expected csv parse_rows output");
}

bool runs_csv_matrix_stdlib() {
    std::error_code error;
    const std::filesystem::path reals = std::filesystem::temp_directory_path() / "dune_vm_csv_reals.csv";
    const std::filesystem::path ints = std::filesystem::temp_directory_path() / "dune_vm_csv_ints.csv";
    const std::filesystem::path bad = std::filesystem::temp_directory_path() / "dune_vm_csv_bad.csv";
    const std::filesystem::path out = std::filesystem::temp_directory_path() / "dune_vm_csv_out.csv";
    std::filesystem::remove(reals, error);
    std::filesystem::remove(ints, error);
    std::filesystem::remove(bad, error);
    std::filesystem::remove(out, error);

    // Everything runs inside one Dune program: the input files are written via
    // fs, read back as typed matrices, round-tripped through write_matrix, and a
    // malformed cell is exercised to confirm the failure message is forwarded.
    const std::string source =
        "import csv; import fs; import matrix; "
        "real_seed: [[real64]] = [[0.0]]; def_r: matrix.Matrix<real64> = matrix.from_rows(real_seed); "
        "int_seed: [[int]] = [[0]]; def_i: matrix.Matrix<int> = matrix.from_rows(int_seed); "
        "fs.write_text(\"" +
        reals.generic_string() +
        "\", \"1.5, 2.0, 3.5\\n4.0, 5.5, 6.0\\n\"); "
        "fs.write_text(\"" +
        ints.generic_string() +
        "\", \"10, 20\\n-30, 40\\n\"); "
        "fs.write_text(\"" +
        bad.generic_string() +
        "\", \"1.0, oops\\n\"); "
        "mr = csv.read_matrix_real64(\"" +
        reals.generic_string() +
        "\"); io.println(mr.is_done()); "
        "m: matrix.Matrix<real64> = mr.value_or(def_r); "
        "io.println(m.rows()); io.println(m.cols()); io.println(m.get(0, 2)); io.println(m.get(1, 1)); "
        "w = csv.write_matrix_real64(\"" +
        out.generic_string() +
        "\", m); io.println(w.is_done()); "
        "again = csv.read_matrix_real64(\"" +
        out.generic_string() +
        "\"); m2: matrix.Matrix<real64> = again.value_or(def_r); io.println(m2.get(0, 0)); io.println(m2.get(1, 2)); "
        "mi = csv.read_matrix_int(\"" +
        ints.generic_string() +
        "\"); mint: matrix.Matrix<int> = mi.value_or(def_i); io.println(mint.get(1, 0)); "
        "b = csv.read_matrix_real64(\"" +
        bad.generic_string() + "\"); io.println(b.is_done()); io.println(b.failure_or(\"no-error\"));";

    const bool passed = expect_eq(run_source(source), "1\n2\n3\n3.5\n5.5\n1\n1.5\n6\n-30\n0\ninvalid number: oops\n",
                                  "expected csv matrix round-trip output");
    std::filesystem::remove(reals, error);
    std::filesystem::remove(ints, error);
    std::filesystem::remove(bad, error);
    std::filesystem::remove(out, error);
    return passed;
}

bool runs_only_test_blocks() {
    // Top-level code must not run under the test runner; each test body runs in
    // declaration order, and top-level functions are in scope inside them.
    const std::string output = run_only_tests("import io;\n"
                                              "io.println(\"toplevel\");\n"
                                              "fn label(): text { return \"body\"; }\n"
                                              "test \"one\" { io.println(label()); }\n"
                                              "test \"two\" { io.println(\"two\"); }");
    return expect_eq(output, "body\ntwo\n", "expected only the test bodies to run, in order");
}

bool test_failure_unwinds() {
    // A runtime panic inside a test body (here an out-of-bounds index) must
    // propagate out of run_test so the runner can mark that test failed.
    try {
        run_only_tests("import io;\ntest \"boom\" { values = [1, 2]; io.println(values[9]); }");
    } catch (const std::runtime_error&) {
        return true;
    }

    std::cerr << "expected a panicking test body to throw out of run_test\n";
    return false;
}

} // namespace

int main() {
    bool passed = true;

    passed = expect_eq(run_source("io.println(40 + 2);"), "42\n", "expected arithmetic output") && passed;
    passed = expect_eq(run_source("message: text = fmt.format(\"{} v{}\", \"Dune\", 1); io.println(message); "
                                  "io.println(fmt.format(\"bits {}\", 0b1010u8));"),
                       "Dune v1\nbits 10\n", "expected format expression and numeric literal output") &&
             passed;
    passed = expect_eq(run_source("io.println(true); io.println(false); io.println(3 > 2); io.println(3 != 3);"), "1\n0\n1\n0\n",
                       "expected boolean and comparison output") &&
             passed;
    passed =
        expect_eq(run_source("x = 10; y = x * 3 - 4 / 2; io.println(y);"), "28\n", "expected variable output") && passed;
    passed = expect_eq(run_source("x = 1; x = x + 2; io.println(x);"), "3\n", "expected reassignment") && passed;
    passed = expect_eq(run_source("x = 3; while x > 0 { x = x - 1; } "
                                  "if x == 0 { io.println(42); } else { io.println(0); }"),
                       "42\n", "expected control flow output") &&
             passed;
    passed = expect_eq(run_source("fn add(a, b) { return a + b; } io.println(add(10, 20));"), "30\n",
                       "expected function call output") &&
             passed;
    passed = expect_eq(run_source("fn add(a: int, b: int): int { return a + b; } "
                                  "fn twice(value: int): int { return add(value, value); } "
                                  "io.println(add(twice(5), add(3, 4)));"),
                       "17\n", "expected nested function call output") &&
             passed;
    passed = expect_eq(run_source("const HIDDEN: int = 7; fn hidden(): int { return HIDDEN; } io.println(hidden());"), "7\n",
                       "expected top-level constant in function output") &&
             passed;
    passed = expect_eq(run_source("fn choose(flag: bool, yes: int, no: int): int { "
                                  "if flag { return yes; } else { return no; } } "
                                  "io.println(choose(false, 1, 2));"),
                       "2\n", "expected function return through branches") &&
             passed;
    passed = expect_eq(run_source("fn show(value: int): int { return value + 1; } "
                                  "fn show(value: bool): int { if value { return 10; } else { return 20; } } "
                                  "io.println(show(41)); io.println(show(false));"),
                       "42\n20\n", "expected overloaded function dispatch") &&
             passed;
    passed = expect_eq(run_source("type Count = int; fn inc(value: Count): Count { return value + 1; } "
                                  "record Point { x: Count, fn new(x: Count): Point { return Point { x: x }; } } "
                                  "type PointAlias = Point; point: PointAlias = Point.new(inc(4)); io.println(point.x);"),
                       "5\n", "expected type alias execution") &&
             passed;
    passed = expect_eq(run_source("small: u8 = 250; wide: uint64 = 10000000000; "
                                  "total: uint64 = wide + 5; io.println(small); io.println(total);"),
                       "250\n10000000005\n", "expected unsigned output") &&
             passed;
    passed =
        expect_eq(run_source("ratio: real = 1 + 2.5; io.println(ratio / 2.0);"), "1.75\n", "expected real output") && passed;
    passed = expect_eq(run_source("mark: glyph = 'Z'; io.println(mark);"), "Z\n", "expected glyph output") && passed;
    passed = expect_eq(run_source("name: text = \"Dune\"; version: int = 1; "
                                  "io.println(fmt.format(\"{} v{}\", name, version)); "
                                  "io.println(fmt.format(\"bool={}, glyph={}, real={}\", true, 'x', 2.5));"),
                       "Dune v1\nbool=1, glyph=x, real=2.5\n", "expected formatted print output") &&
             passed;
    passed = expect_eq(run_source(R"dune(path: text = r"C:\Users\name\data.csv";
io.println(path);
io.println("hello\nworld");
io.println("quote \"ok\"");
io.println("slash \\ ok");
io.println('\n' to int);
io.println('\t' to int);
io.println('\r' to int);
io.println('\\');
io.println('\'');
io.println('\0' to int);)dune"),
                       "C:\\Users\\name\\data.csv\n"
                       "hello\nworld\n"
                       "quote \"ok\"\n"
                       "slash \\ ok\n"
                       "10\n9\n13\n\\\n'\n0\n",
                       "expected raw strings and escape output") &&
             passed;
    passed = expect_error_contains(R"(bad: text = "\x";)", R"(unknown text escape '\x')",
                                   "expected invalid text escape diagnostic") &&
             passed;
    passed = expect_eq(run_source("fn log(message: text): unit { io.println(message); return; } "
                                  "fn noop(): unit { } "
                                  "tiny: i8 = 127; small: i16 = 32767; mid: i32 = 2147483647; "
                                  "wide: i64 = 9000000000; index: usize = 5; offset: isize = 6; "
                                  "rough: real32 = 1 + 2.5; exact: real64 = 2.25; "
                                  "log(\"types\"); noop(); io.println(tiny); io.println(small); io.println(mid); "
                                  "io.println(wide); io.println(index); io.println(offset); io.println(rough); io.println(exact); "
                                  "if \"same\" == \"same\" { io.println(\"same\"); } else { io.println(\"bad\"); }"),
                       "types\n127\n32767\n2147483647\n9000000000\n5\n6\n3.5\n2.25\nsame\n",
                       "expected standard type output") &&
             passed;
    passed = expect_eq(run_source("import math; "
                                  "values: [int] = [1, math.square(2), 5]; "
                                  "base: u64 = 7; "
                                  "values.push(math.square(values[2])); "
                                  "io.println(values.len()); io.println(values[1]); io.println(values[3]); "
                                  "io.println(math.square(base)); io.println(math.square(1.5)); "
                                  "io.println(math.abs(0 - 8)); io.println(math.cube(3)); io.println(math.min(4, 9)); "
                                  "io.println(math.max(4, 9)); io.println(math.clamp(12, 0, 10)); "
                                  "rough: real32 = 1.5; io.println(math.max(rough, 2.5)); "
                                  "io.println(math.abs(0.0 - 2.5)); io.println(math.round(math.PI)); "
                                  "io.println(math.floor(2.9)); io.println(math.ceil(2.1)); io.println(math.sqrt(9.0)); "
                                  "io.println(math.pow(2.0, 3)); io.println(math.sin(0.0)); io.println(math.cos(0.0)); "
                                  "io.println(math.tan(0.0)); io.println(math.exp(0.0)); io.println(math.ln(1.0)); "
                                  "io.println(math.normalize_radians(math.TAU));"),
                       "4\n4\n25\n49\n2.25\n8\n27\n4\n9\n10\n2.5\n2.5\n3\n2\n3\n3\n8\n0\n1\n0\n1\n0\n0\n",
                       "expected arrays and module output") &&
             passed;
    passed = expect_eq(run_source("value = 17; io.println(0 - value); io.println(-value); io.println(17 % 5); "
                                  "io.println(!false); io.println(false && (1 / 0 == 0)); io.println(true || (1 / 0 == 0)); "
                                  "count: real64 = value to real64; io.println(count / 2.0); "
                                  "code: int = 'A' to int; letter: glyph = 66 to glyph; "
                                  "flag: bool = 0 to bool; io.println(code); io.println(letter); io.println(flag); "
                                  "values: [int] = [1, 2]; io.println(values.is_empty()); values.push(3); "
                                  "io.println(values.pop()); io.println(values.len()); values.clear(); io.println(values.is_empty()); "
                                  "message: text = \"dune language\"; io.println(message.len()); "
                                  "io.println(message.contains(\"lang\")); io.println(message.starts_with(\"dune\")); "
                                  "io.println(\"\".is_empty());"),
                       "-17\n-17\n2\n1\n0\n1\n8.5\n65\nB\n0\n0\n3\n2\n1\n13\n1\n1\n1\n",
                       "expected operators casts and methods output") &&
             passed;
    passed = expect_eq(run_source("foreign fn c_sqrt(value: real64): real64 = \"sqrt\"; "
                                  "message: text = \"dune language\"; io.println(message[0]); "
                                  "io.println(message[5:13]); io.println(message[:4]); io.println(message[5:]); "
                                  "values: [int] = [1, 2, 3, 4, 5]; middle: [int] = values[1:4]; "
                                  "io.println(middle.len()); io.println(middle[0]); io.println(middle[2]); "
                                  "total = 0; "
                                  "for i = 0; i < 6; i = i + 1 { "
                                  "if i == 1 { continue; } if i == 4 { break; } total = total + i; } "
                                  "io.println(total); io.println(c_sqrt(81.0));"),
                       "d\nlanguage\ndune\nlanguage\n3\n2\n4\n5\n9\n",
                       "expected foreign functions slices text indexing and for loop output") &&
             passed;
    passed = expect_eq(run_source("values: [int] = [1, 2, 3, 4]; total = 0; "
                                  "for value in values { if value == 3 { continue; } total = total + value; } "
                                  "for i in 0..values.len() { if i == 2 { break; } total = total + values[i]; } "
                                  "for empty in 5..2 { total = total + empty; } io.println(total);"),
                       "10\n", "expected for-in arrays ranges break continue and empty range output") &&
             passed;
    passed = expect_eq(run_source("import array; import text; "
                                  "values: [int] = [1, 2, 3]; reversed: [int] = array.reverse(values); "
                                  "io.println(array.sum(reversed)); io.println(array.first(reversed)); "
                                  "io.println(array.last(reversed)); io.println(array.contains(values, 2)); "
                                  "io.println(array.index_of(values, 3)); "
                                  "io.println(values.first()); io.println(values.append(4).last()); "
                                  "ranged: [int] = array.range(2, 5); io.println(array.sum(ranged)); "
                                  "message: text = \" dune language \"; io.println(text.trim(message)); "
                                  "io.println(text.ends_with(text.trim(message), \"age\")); "
                                  "io.println(message.trim().ends_with(\"age\")); "
                                  "io.println(text.index_of(message, 'l')); io.println(text.count(message, 'a')); "
                                  "io.println(text.is_digit('7')); io.println(text.is_alpha('Z'));"),
                       "6\n3\n1\n1\n2\n1\n4\n9\ndune language\n1\n1\n6\n2\n1\n1\n",
                       "expected array and text stdlib module output") &&
             passed;
    passed = expect_eq(run_source("import array; import math; "
                                  "fn identity<T>(value: T): T { return value; } "
                                  "fn twice<T is numeric>(value: T): T { return value + value; } "
                                  "words: [text] = [\"dune\", \"lang\"]; "
                                  "reversed: [text] = array.reverse(words); "
                                  "rough: real32 = 1.5; "
                                  "io.println(identity(42)); io.println(identity(\"done\")); io.println(reversed.first()); "
                                  "io.println(math.square(12)); io.println(math.square(rough)); io.println(twice(9));"),
                       "42\ndone\nlang\n144\n2.25\n18\n", "expected generic functions and generic stdlib output") &&
             passed;
    passed = expect_eq(run_source("// comments are ignored\n"
                                  "import array; // stdlib receiver methods\n"
                                  "values: [int] = [7, 8];\n"
                                  "io.println(values[0]); // arrays are zero-based\n"
                                  "io.println(values.first());\n"
                                  "io.println(8 / 2);"),
                       "7\n7\n4\n", "expected comments and zero-based array output") &&
             passed;
    passed = expect_eq(run_source("record Point { x: int, y: int, "
                                  "fn sum(): int { return this.x + this.y; } } "
                                  "fn make(x: int, y: int): Point { return Point { x: x, y: y }; } "
                                  "p: Point = make(10, 20); io.println(p.x); io.println(p.y); io.println(p.sum());"),
                       "10\n20\n30\n", "expected record fields and methods output") &&
             passed;
    passed = expect_eq(run_source("record Box<T> { value: T, fn value_or(default: T): T { return this.value; } } "
                                  "fn boxed<T>(value: T): Box<T> { return Box { value: value }; } "
                                  "number: Box<int> = boxed(7); label: Box<text> = boxed(\"done\"); "
                                  "io.println(number.value_or(0)); io.println(label.value_or(\"bad\")); "
                                  "io.println(when number.value { is 7 { 70 } is _ { 0 } }); "
                                  "io.println(when \"dune\" { is \"lang\" { 1 } is _ { 2 } });"),
                       "7\ndone\n70\n2\n", "expected generic records and when output") &&
             passed;
    passed = expect_eq(run_source("choice Maybe { Present(int), Absent, } "
                                  "fn unwrap(value: Maybe): int { return when value { Present(x) => x + 1; "
                                  "Absent => 0; }; } "
                                  "value: Maybe = Present(41); missing: Maybe = Absent; "
                                  "io.println(unwrap(value)); io.println(unwrap(missing)); "
                                  "io.println(when missing { Present(x) => x; _ => 7; }); "
                                  "io.println(when 2 { 1 => 10; _ => 20; });"),
                       "42\n0\n7\n20\n", "expected arrow-style pattern matching output") &&
             passed;
    passed = expect_eq(run_source("record Point { x: int, y: int } "
                                  "fn minmax(values: [int]): (int, int) { return (values[0], values[1]); } "
                                  "(lo, hi) = minmax([3, 8]); io.println(lo); io.println(hi); "
                                  "point: Point = Point { x: lo, y: hi }; "
                                  "io.println(when point { Point { x, y } => x + y; }); "
                                  "io.println(when (lo, hi) { (left, right) => left * right; });"),
                       "3\n8\n11\n24\n", "expected tuple and record destructuring output") &&
             passed;
    passed = expect_eq(run_source("contract Shape { area(): real64; } "
                                  "record Circle with Shape { radius: real64, "
                                  "fn new(radius: real64): Circle { return Circle { radius: radius }; } "
                                  "fn area(): real64 { return 3.0 * this.radius * this.radius; } } "
                                  "fn area_of<T is Shape>(shape: T): real64 { return shape.area(); } "
                                  "circle: Circle = Circle.new(2.0); "
                                  "io.println(circle.area()); io.println(area_of(circle));"),
                       "12\n12\n", "expected constructors, contracts, and static contract-bound calls") &&
             passed;
    passed = expect_eq(run_source("record Point { x: int, y: int, "
                                  "fn to_text(): text { return fmt.format(\"({}, {})\", this.x, this.y); } } "
                                  "p: Point = Point { x: 1, y: 2 }; "
                                  "io.println(p); io.println(fmt.format(\"p = {}\", p)); "
                                  "io.println(fmt.format(\"[{}]\", p));"),
                       "(1, 2)\np = (1, 2)\n[(1, 2)]\n", "expected Display record printing via to_text") &&
             passed;
    passed = expect_eq(run_source("import display; "
                                  "record Tag with display.Display { name: text, "
                                  "fn to_text(): text { return fmt.format(\"#{}\", this.name); } } "
                                  "t: Tag = Tag { name: \"note\" }; "
                                  "io.println(t); io.println(display.show(t));"),
                       "#note\n#note\n", "expected Display contract print and show helper") &&
             passed;
    passed = expect_eq(run_source("record Point derive eq, copy, debug { x: int, y: int } "
                                  "a: Point = Point { x: 1, y: 2 }; b: Point = Point { x: 1, y: 2 }; "
                                  "c: Point = Point { x: 9, y: 9 }; d: Point = a.copy(); "
                                  "io.println(a); io.println(a.equals(b)); io.println(a.equals(c)); io.println(d.equals(a));"),
                       "Point(x: 1, y: 2)\n1\n0\n1\n", "expected derive eq/copy/debug output") &&
             passed;
    passed = expect_eq(run_source("record Wrap<T> derive eq, copy { value: T } "
                                  "one: Wrap<int> = Wrap { value: 7 }; two: Wrap<int> = one.copy(); "
                                  "io.println(one.equals(two)); io.println(two.value);"),
                       "1\n7\n", "expected derive on generic record") &&
             passed;
    passed = expect_eq(run_source("record Point derive eq, debug { x: int, y: int, "
                                  "fn to_text(): text { return fmt.format(\"P{},{}\", this.x, this.y); } } "
                                  "p: Point = Point { x: 3, y: 4 }; io.println(p);"),
                       "P3,4\n", "expected explicit to_text to override derive debug") &&
             passed;
    passed = expect_eq(run_source("choice Maybe { Present(int), Absent, } "
                                  "x = 99; value: Maybe = Present(30); missing: Maybe = Absent; "
                                  "io.println(when value { is Present(x) { x + 1 } is Absent { 0 } }); "
                                  "io.println(when missing { is Present(x) { x } is Absent { 7 } }); io.println(x);"),
                       "31\n7\n99\n", "expected choice variants and scoped when bindings") &&
             passed;
    passed = expect_eq(run_source("x = 1; { x: int = 2; io.println(x); } io.println(x); "
                                  "total = 0; for i = 0; i < 3; i = i + 1 { total = total + i; } io.println(total); "
                                  "choice Maybe { Present(int), Absent, } value: Maybe = Present(5); "
                                  "io.println(when value { is Present(x) { x } is Absent { 0 } }); io.println(x);"),
                       "2\n1\n3\n5\n1\n", "expected lexical scope output") &&
             passed;
    passed = expect_eq(run_source("record Point { x: int, y: int } "
                                  "values: [int] = [1, 2]; values[1] = 9; io.println(values[1]); "
                                  "grid: [[int]] = [[1, 2], [3, 4]]; grid[1][0] = 8; io.println(grid[1][0]); "
                                  "point: Point = Point { x: 1, y: 2 }; point.x = 7; io.println(point.x); "
                                  "points: [Point] = [Point { x: 3, y: 4 }]; points[0].y = 11; io.println(points[0].y);"),
                       "9\n8\n7\n11\n", "expected assignment target output") &&
             passed;
    passed = expect_eq(run_source("record Point { x: int, y: int } "
                                  "values: [int] = [1]; alias = values; alias[0] = 2; io.println(values[0]); "
                                  "const frozen: [int] = [5]; mutable_alias = frozen; mutable_alias[0] = 6; "
                                  "io.println(frozen[0]); "
                                  "point: Point = Point { x: 3, y: 0 }; point_alias = point; point_alias.x = 4; "
                                  "io.println(point.x); "
                                  "const fixed_point: Point = Point { x: 7, y: 0 }; "
                                  "mutable_point_alias = fixed_point; mutable_point_alias.x = 9; "
                                  "io.println(fixed_point.x); "
                                  "scalar = 1; scalar_copy = scalar; scalar_copy = 2; "
                                  "io.println(scalar); io.println(scalar_copy);"),
                       "2\n6\n4\n9\n1\n2\n", "expected mutability and aliasing output") &&
             passed;
    passed = expect_eq(run_source("import maybe; import outcome; import assert; import collections; "
                                  "maybe_value: maybe.Maybe<int> = maybe.present(42); "
                                  "missing: maybe.Maybe<int> = maybe.absent(0); "
                                  "failed: outcome.Outcome<int, text> = outcome.failed(0, \"bad\"); "
                                  "repeated: [int] = collections.repeat_int(3, 4); "
                                  "io.println(maybe_value.value_or(0)); io.println(missing.value_or(7)); "
                                  "io.println(failed.failure_or(\"absent\")); io.println(repeated.len()); "
                                  "io.println(assert.equals_int(repeated[0], 3));"),
                       "42\n7\nbad\n4\n1\n", "expected record stdlib module output") &&
             passed;
    passed = expect_eq(run_source("record Config { count: int = 7, name: text = \"default\", values: [int] = [1] } "
                                  "first: Config = Config {}; second: Config = Config {}; "
                                  "second.values.push(2); overridden: Config = Config { count: 3 }; "
                                  "io.println(first.count); io.println(first.name); io.println(first.values.len()); "
                                  "io.println(second.values.len()); io.println(overridden.count); io.println(overridden.name);"),
                       "7\ndefault\n1\n2\n3\ndefault\n", "expected record field default output") &&
             passed;
    passed = expect_eq(run_source("import autograd; "
                                  "x = autograd.variable(2.0); y = autograd.variable(3.0); "
                                  "loss = x.mul(y).add(x.pow(2.0)).add(1.0); loss.backward(); "
                                  "io.println(loss.data); io.println(x.grad); io.println(y.grad); "
                                  "a = autograd.variable(3.0); shared = a.mul(a); doubled = shared.add(shared); "
                                  "doubled.backward(); io.println(doubled.data); io.println(a.grad); "
                                  "doubled.backward(); io.println(a.grad); "
                                  "negative = autograd.variable(0.0 - 2.0); clipped = negative.relu(); "
                                  "clipped.backward(); io.println(clipped.data); io.println(negative.grad);"),
                       "11\n7\n2\n18\n12\n12\n0\n0\n", "expected scalar autograd output") &&
             passed;
    passed = expect_eq(run_source("import matrix; "
                                  "v = matrix.vector([1, 2, 3]); w = matrix.vector([4, 5, 6]); "
                                  "io.println(v.add(w).get(0)); io.println(v.dot(w)); io.println(v.rsub(10).get(0)); "
                                  "io.println(v.mul(w).get(1)); io.println(v.concat(w).len()); "
                                  "m = matrix.from_flat(2, 3, [1, 2, 3, 4, 5, 6]); "
                                  "n = matrix.from_flat(3, 2, [7, 8, 9, 10, 11, 12]); "
                                  "product = m.matmul(n); io.println(product.get(0, 0)); io.println(product.get(1, 1)); "
                                  "io.println(m.trace()); io.println(m.flatten().get(4)); "
                                  "z: matrix.Matrix<int> = matrix.zeros(2, 2); io.println(z.sum()); "
                                  "eye: matrix.Matrix<int> = matrix.eye(2); io.println(eye.trace()); "
                                  "r = matrix.vector([1.5, 2.5]); io.println(r.sum()); "
                                  "io.println(v.norm_squared()); io.println(v.outer(w).get(2, 1)); "
                                  "io.println(v.matmul(n).get(1)); rows = matrix.from_rows([[1, 2], [3, 4]]); "
                                  "io.println(rows.det2()); io.println(rows.mean()); io.println(rows.sum_rows().get(1)); "
                                  "io.println(rows.mean_columns().get(1));"),
                       "5\n32\n9\n10\n6\n58\n154\n6\n5\n0\n2\n4\n14\n15\n64\n-2\n2.5\n7\n3\n",
                       "expected generic matrix stdlib output") &&
             passed;
    passed = expect_error_contains("import matrix; left = matrix.vector([1, 2]); "
                                   "right = matrix.vector([1, 2, 3]); io.println(left.dot(right));",
                                   "vector shape mismatch", "expected vector shape diagnostic") &&
             passed;
    passed = expect_error_contains("import matrix; io.println(matrix.from_flat(2, 2, [1, 2, 3]).sum());",
                                   "matrix data length mismatch", "expected matrix constructor shape diagnostic") &&
             passed;
    passed = expect_error_contains("import runtime; runtime.panic(\"boom\");", "boom",
                                   "expected runtime panic diagnostic") &&
             passed;
    passed = expect_eq(run_source("import array; "
                                  "values = [1, 2, 3]; io.println(values.sum()); io.println(values.product()); "
                                  "io.println(values.min()); io.println(values.max()); io.println(array.range(2, 9, 3).sum()); "
                                  "combined = values.concat([4]).prepend(0); io.println(combined.first()); "
                                  "io.println(combined.last()); io.println(combined.slice(1, 3).sum()); "
                                  "flags = [true, false]; io.println(flags.all()); io.println(flags.any());"),
                       "6\n6\n1\n3\n15\n0\n4\n3\n0\n1\n", "expected expanded array stdlib output") &&
             passed;
    passed =
        expect_eq(run_source("record Optimizer { lr: real64, momentum: real64, "
                             "static fn default(): Optimizer { return Optimizer { lr: 0.01, momentum: 0.0 }; } "
                             "static fn with_lr(lr: real64): Optimizer { return Optimizer { lr: lr, momentum: 0.0 }; } "
                             "fn rate(): real64 { return this.lr; } } "
                             "opt: Optimizer = Optimizer.default(); io.println(opt.rate()); "
                             "tuned: Optimizer = Optimizer.with_lr(0.2); io.println(tuned.rate());"),
                  "0.01\n0.2\n", "expected static associated function output") &&
        passed;
    passed = expect_eq(run_source("values = [1, 2, 3]; words = [\"dune\", \"lang\"]; "
                                  "message: text = \"dune language\"; enabled: bool = true; "
                                  "io.println(2 in values); io.println(4 in values); io.println(\"lang\" in words); "
                                  "io.println(\"lang\" in message); io.println(\"go\" in message); "
                                  "if 1 + 1 in values && enabled { io.println(99); }"),
                       "1\n0\n1\n1\n0\n99\n", "expected membership operator output") &&
             passed;
    passed = expect_throws("io.println(missing);", "expected undefined variable to throw") && passed;
    passed =
        expect_eq(run_source("missing = 1; io.println(missing);"), "1\n", "expected first assignment to bind") && passed;
    passed = expect_throws("io.println(1 / 0);", "expected division by zero to throw") && passed;
    passed = expect_error_contains("values: [int] = [1]; io.println(values[2]);", "array index out of bounds",
                                   "expected array bounds error") &&
             passed;
    passed = expect_error_contains("message: text = \"done\"; io.println(message[4]);", "text index out of bounds",
                                   "expected text bounds error") &&
             passed;
    passed =
        expect_error_contains("values: [int] = [1, 2]; bad: [int] = values[2:1];",
                              "slice start cannot be greater than slice end", "expected invalid slice range error") &&
        passed;
    passed =
        expect_error_contains("x: int = true;", "expected type 'int' but got 'bool'", "expected static type error") &&
        passed;

    passed = runs_process_stdlib() && passed;
    passed = runs_io_stdlib() && passed;
    passed = runs_fs_stdlib() && passed;
    passed = runs_csv_stdlib() && passed;
    passed = runs_csv_matrix_stdlib() && passed;

    // First-class function values: the acceptance-criteria chain plus the other
    // higher-order stdlib methods, and a callback that maps between types.
    passed = expect_eq(run_source("import array; "
                                  "fn square(x: int): int { return x * x; } "
                                  "fn is_positive(x: int): bool { return x > 0; } "
                                  "fn add(acc: int, x: int): int { return acc + x; } "
                                  "fn is_even(x: int): bool { return x % 2 == 0; } "
                                  "values = [0 - 3, 2, 0 - 1, 4, 5]; "
                                  "io.println(values.filter(is_positive).map(square).sum()); "
                                  "io.println(values.fold(0, add)); "
                                  "io.println([1, 2, 3, 4].reduce(add)); "
                                  "io.println(values.any(is_positive)); "
                                  "io.println(values.all(is_positive)); "
                                  "io.println(values.count_where(is_positive)); "
                                  "io.println([1, 2, 3, 4].map(is_even).count_value(true));"),
                       "45\n7\n10\n1\n0\n3\n2\n", "expected higher-order array stdlib output") &&
             passed;
    // A function value flowing through a user-defined higher-order method with no
    // stdlib involved, exercising indirect calls in the VM directly.
    passed = expect_eq(run_source("fn twice(x: int): int { return x + x; } "
                                  "method<T, U> [T].transform(f: fn(T): U): [U] { "
                                  "result: [U] = []; "
                                  "for i = 0; i < this.len(); i = i + 1 { result.push(f(this[i])); } "
                                  "return result; } "
                                  "out = [10, 20].transform(twice); io.println(out[0]); io.println(out[1]);"),
                       "20\n40\n", "expected user-defined higher-order method output") &&
             passed;

    // Modules v2: aliased and selective / grouped stdlib imports resolve to the
    // same canonical symbols at run time.
    passed = expect_eq(run_source("import math as m; from array import range, sum; "
                                  "io.println(sum(range(1, 5))); io.println(m.square(4)); "
                                  "io.println(m.max(3, 7));"),
                       "10\n16\n7\n", "expected Modules v2 alias/selective runtime output") &&
             passed;

    passed = runs_only_test_blocks() && passed;
    passed = test_failure_unwinds() && passed;

    return passed ? 0 : 1;
}
