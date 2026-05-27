#include "lsp/lsp_server.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }

    return true;
}

std::string lsp_message(const std::string& body) {
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

bool diagnoses_valid_source() {
    const std::vector<dune::lsp::Diagnostic> diagnostics =
        dune::lsp::diagnose_source("fn add(a: int, b: int) -> int { return a + b; } "
                                   "let total: int = add(10, 20);");

    return expect(diagnostics.empty(), "expected no diagnostics for valid source");
}

bool diagnoses_type_errors_with_range() {
    const std::vector<dune::lsp::Diagnostic> diagnostics = dune::lsp::diagnose_source("let x: int = true;");

    bool passed = true;
    passed = expect(diagnostics.size() == 1, "expected one diagnostic") && passed;
    if (!diagnostics.empty()) {
        passed = expect(diagnostics[0].line == 1, "expected diagnostic line") && passed;
        passed = expect(diagnostics[0].start_column == 14, "expected diagnostic start column") && passed;
        passed = expect(diagnostics[0].end_column == 17, "expected diagnostic end column") && passed;
        passed = expect(diagnostics[0].message.find("expected type 'int' but got 'bool'") != std::string::npos,
                        "expected type mismatch message") &&
                 passed;
    }

    return passed;
}

bool publishes_lsp_diagnostics() {
    const std::string uri = "file:///tmp/bad.dn";
    const std::string initialize = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string opened = R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")" +
                               uri + R"(","languageId":"dune","version":1,"text":"let x: int = true;"}}})";
    const std::string shutdown = R"({"jsonrpc":"2.0","id":2,"method":"shutdown","params":null})";
    const std::string exit = R"({"jsonrpc":"2.0","method":"exit","params":null})";

    std::istringstream input(lsp_message(initialize) + lsp_message(opened) + lsp_message(shutdown) + lsp_message(exit));
    std::ostringstream output;
    dune::lsp::run(input, output);
    const std::string text = output.str();

    bool passed = true;
    passed = expect(text.find("\"capabilities\"") != std::string::npos, "expected initialize response") && passed;
    passed = expect(text.find("textDocument/publishDiagnostics") != std::string::npos,
                    "expected diagnostics notification") &&
             passed;
    passed =
        expect(text.find("expected type 'int' but got 'bool'") != std::string::npos, "expected diagnostic message") &&
        passed;
    passed = expect(text.find("\"severity\":1") != std::string::npos, "expected error severity") && passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = diagnoses_valid_source() && passed;
    passed = diagnoses_type_errors_with_range() && passed;
    passed = publishes_lsp_diagnostics() && passed;
    return passed ? 0 : 1;
}
