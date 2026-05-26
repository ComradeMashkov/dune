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
    passed = expect_error_contains("let x: int = true;", "expected type 'int' but got 'bool'",
                                   "expected let type mismatch") &&
             passed;
    passed = expect_error_contains("let x: bool = true; x = 1;", "expected type 'bool' but got 'int'",
                                   "expected assignment type mismatch") &&
             passed;
    passed = expect_error_contains("print(true + 1);", "expected type 'int' but got 'bool'",
                                   "expected invalid binary operation") &&
             passed;
    passed = expect_error_contains("fn bad() -> bool { return 1; }", "expected type 'bool' but got 'int'",
                                   "expected return type mismatch") &&
             passed;
    passed = expect_error_contains("fn is_ok(value: bool) -> bool { return value; } print(is_ok(1));",
                                   "argument 1 for function 'is_ok': expected type 'bool' but got 'int'",
                                   "expected call argument mismatch") &&
             passed;

    return passed ? 0 : 1;
}
