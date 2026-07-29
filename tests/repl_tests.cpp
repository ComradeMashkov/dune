#include "repl/repl.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

struct ReplResult {
    int status = 0;
    std::string output;
    std::string error;
};

ReplResult run_repl(const std::string& source, bool show_prompts = false) {
    std::istringstream input(source);
    std::ostringstream output;
    std::ostringstream error;
    const int status = dune::repl::run(input, output, error,
                                       dune::repl::Options{"test", std::filesystem::current_path(), show_prompts});
    return ReplResult{status, output.str(), error.str()};
}

bool keeps_language_state_and_recovers_from_type_errors() {
    const ReplResult result = run_repl(R"dune(x = 40 + 2;
x
x = x + 1;
x
import math;
math.square(9)
fn add(
    a: int,
    b: int
): int {
    return a + b;
}
record Box<T> {
    value: T,
}
record Label {
    value: text,
    fn to_text(): text {
        return this.value;
    }
}
type IntBox = Box<int>;
choice Maybe<T> {
    Some(T),
    None,
}
box: IntBox = Box { value: add(x, 1) };
box.value
label: Label = Label { value: "ready" };
label
maybe: Maybe<int> = Some(box.value);
when maybe {
    is Some(value) { value }
    is None { 0 }
}
1 + ;
bad: int = true;
add(x, 2)
:quit
)dune");

    bool passed = true;
    passed = expect(result.status == 0, "expected a successful REPL session") && passed;
    passed = expect(result.output == "Dune test\nType :help for help.\n42\n43\n81\n44\nready\n44\n45\n",
                    "expected bindings, imports, functions, records, choices, aliases, and expressions") &&
             passed;
    passed = expect(result.error.find("expected type 'int' but got 'bool'") != std::string::npos,
                    "expected the type error to be reported") &&
             passed;
    passed = expect(result.error.find("expected expression") != std::string::npos,
                    "expected the parser error to be reported") &&
             passed;
    passed = expect(result.error.find("<repl>") != std::string::npos, "expected a REPL source snippet") && passed;
    return passed;
}

bool supports_commands_and_reset() {
    const ReplResult result = run_repl("value = 7;\n:help\n:unknown\n:reset\nvalue\n:quit\n");

    bool passed = true;
    passed = expect(result.status == 0, "expected commands not to terminate the REPL") && passed;
    passed = expect(result.output.find("Commands:\n") != std::string::npos, "expected :help output") && passed;
    passed =
        expect(result.output.find("session reset\n") != std::string::npos, "expected :reset confirmation") && passed;
    passed = expect(result.error.find("unknown REPL command ':unknown'") != std::string::npos,
                    "expected an unknown-command diagnostic") &&
             passed;
    passed = expect(result.error.find("undefined variable 'value'") != std::string::npos,
                    "expected :reset to clear prior bindings") &&
             passed;
    return passed;
}

bool recovers_from_runtime_errors() {
    const ReplResult result = run_repl("import io;\nx = 1;\nio.println(1 / 0);\nx + 1\n:quit\n");

    bool passed = true;
    passed = expect(result.status == 0, "expected runtime failure recovery") && passed;
    passed = expect(result.output == "Dune test\nType :help for help.\n2\n",
                    "expected the session to continue after a runtime error") &&
             passed;
    passed = expect(result.error.find("division by zero") != std::string::npos,
                    "expected the runtime error to be reported") &&
             passed;
    return passed;
}

bool renders_interactive_prompts_and_reports_incomplete_eof() {
    const ReplResult prompted = run_repl("fn answer(): int {\nreturn 42;\n}\nanswer()\n:quit\n", true);
    const ReplResult incomplete = run_repl("fn unfinished(): int {\n");

    bool passed = true;
    passed = expect(prompted.status == 0, "expected prompted session success") && passed;
    passed = expect(prompted.output.find("> ... ... > 42\n> ") != std::string::npos,
                    "expected primary and continuation prompts") &&
             passed;
    passed = expect(incomplete.status == 1, "expected incomplete piped input to fail") && passed;
    passed = expect(incomplete.error == "error: incomplete input\n", "expected incomplete-input diagnostic") && passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = keeps_language_state_and_recovers_from_type_errors() && passed;
    passed = supports_commands_and_reset() && passed;
    passed = recovers_from_runtime_errors() && passed;
    passed = renders_interactive_prompts_and_reports_incomplete_eof() && passed;
    return passed ? 0 : 1;
}
