#include "compiler/compiler.hpp"
#include "lexer/lexer.hpp"
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
    dune::Compiler compiler;
    dune::VirtualMachine vm(compiler.compile(parser.parse()));

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
        expect_eq(run_source("let x = 10; let y = x * 3 - 4 / 2; print(y);"), "28\n", "expected variable output") &&
        passed;
    passed = expect_eq(run_source("let x = 1; let x = x + 2; print(x);"), "3\n", "expected reassignment") && passed;
    passed = expect_eq(run_source("let x = 3; while x > 0 { x = x - 1; } "
                                  "if x == 0 { print(42); } else { print(0); }"),
                       "42\n", "expected control flow output") &&
             passed;
    passed = expect_eq(run_source("fn add(a, b) { return a + b; } print(add(10, 20));"), "30\n",
                       "expected function call output") &&
             passed;
    passed = expect_eq(run_source("fn add(a: int, b: int) -> int { return a + b; } "
                                  "fn twice(value: int) -> int { return add(value, value); } "
                                  "print(add(twice(5), add(3, 4)));"),
                       "17\n", "expected nested function call output") &&
             passed;
    passed = expect_eq(run_source("fn choose(flag: bool, yes: int, no: int) -> int { "
                                  "if flag { return yes; } else { return no; } } "
                                  "print(choose(false, 1, 2));"),
                       "2\n", "expected function return through branches") &&
             passed;
    passed = expect_throws("print(missing);", "expected undefined variable to throw") && passed;
    passed = expect_throws("missing = 1;", "expected undefined assignment to throw") && passed;
    passed = expect_throws("print(1 / 0);", "expected division by zero to throw") && passed;
    passed = expect_error_contains("let x: int = true;", "expected type 'int' but got 'bool'",
                                   "expected static type error") &&
             passed;

    return passed ? 0 : 1;
}
