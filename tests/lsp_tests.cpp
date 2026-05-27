#include "lsp/lsp_server.hpp"

#include <iostream>
#include <optional>
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

bool has_completion(const std::vector<dune::lsp::CompletionItem>& completions, const std::string& label) {
    for (const dune::lsp::CompletionItem& completion : completions) {
        if (completion.label == label) {
            return true;
        }
    }

    return false;
}

bool diagnoses_valid_source() {
    const std::vector<dune::lsp::Diagnostic> diagnostics =
        dune::lsp::diagnose_source("add(a: int, b: int): int { return a + b; } "
                                   "const HIDDEN: int = 7; hidden(): int { return HIDDEN; } "
                                   "total: int = add(10, hidden());");

    return expect(diagnostics.empty(), "expected no diagnostics for valid source");
}

bool diagnoses_type_errors_with_range() {
    const std::vector<dune::lsp::Diagnostic> diagnostics = dune::lsp::diagnose_source("x: int = true;");

    bool passed = true;
    passed = expect(diagnostics.size() == 1, "expected one diagnostic") && passed;
    if (!diagnostics.empty()) {
        passed = expect(diagnostics[0].line == 1, "expected diagnostic line") && passed;
        passed = expect(diagnostics[0].start_column == 10, "expected diagnostic start column") && passed;
        passed = expect(diagnostics[0].end_column == 13, "expected diagnostic end column") && passed;
        passed = expect(diagnostics[0].message.find("expected type 'int' but got 'bool'") != std::string::npos,
                        "expected type mismatch message") &&
                 passed;
    }

    return passed;
}

bool completes_keywords_and_local_symbols() {
    const std::vector<dune::lsp::CompletionItem> completions =
        dune::lsp::complete_source("add(a: int, b: int): int { return a + b; }\ntotal: int = add(10, 20);");

    bool passed = true;
    passed = expect(has_completion(completions, "method"), "expected keyword completion") && passed;
    passed = expect(has_completion(completions, "real64"), "expected type completion") && passed;
    passed = expect(has_completion(completions, "add"), "expected function completion") && passed;
    passed = expect(has_completion(completions, "total"), "expected local variable completion") && passed;
    return passed;
}

bool completes_imported_module_members() {
    const std::vector<dune::lsp::CompletionItem> completions =
        dune::lsp::complete_source("import math;\nprint(math.);", {}, {}, 1, 11);

    bool passed = true;
    passed = expect(has_completion(completions, "square"), "expected module function completion") && passed;
    passed = expect(has_completion(completions, "PI"), "expected module constant completion") && passed;
    return passed;
}

bool hovers_local_symbols() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source("total: int = 42;\nprint(total);", {}, {}, 1, 7);

    bool passed = true;
    passed = expect(hover.has_value(), "expected hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("total: int") != std::string::npos, "expected variable hover") && passed;
    }
    return passed;
}

bool hovers_imported_module_members() {
    const std::optional<dune::lsp::Hover> hover =
        dune::lsp::hover_source("import math;\nprint(math.square(5));", {}, {}, 1, 13);

    bool passed = true;
    passed = expect(hover.has_value(), "expected module member hover") && passed;
    if (hover.has_value()) {
        passed = expect(hover->contents.find("square<T is numeric>(value: T): T") != std::string::npos,
                        "expected module function hover") &&
                 passed;
    }
    return passed;
}

bool publishes_lsp_diagnostics() {
    const std::string uri = "file:///tmp/bad.dn";
    const std::string initialize = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string opened = R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")" +
                               uri + R"(","languageId":"dune","version":1,"text":"x: int = true;"}}})";
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

bool serves_lsp_completions_and_hover() {
    const std::string uri = "file:///tmp/main.dn";
    const std::string source = "import math;\\ntotal: int = 42;\\nprint(math.);\\nprint(total);";
    const std::string initialize = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string opened = R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":")" +
                               uri + R"(","languageId":"dune","version":1,"text":")" + source + R"("}}})";
    const std::string completion =
        R"({"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":")" + uri +
        R"("},"position":{"line":2,"character":11}}})";
    const std::string hover =
        R"({"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{"textDocument":{"uri":")" + uri +
        R"("},"position":{"line":3,"character":8}}})";
    const std::string shutdown = R"({"jsonrpc":"2.0","id":4,"method":"shutdown","params":null})";
    const std::string exit = R"({"jsonrpc":"2.0","method":"exit","params":null})";

    std::istringstream input(lsp_message(initialize) + lsp_message(opened) + lsp_message(completion) +
                             lsp_message(hover) + lsp_message(shutdown) + lsp_message(exit));
    std::ostringstream output;
    dune::lsp::run(input, output);
    const std::string text = output.str();

    bool passed = true;
    passed =
        expect(text.find("\"completionProvider\"") != std::string::npos, "expected completion capability") && passed;
    passed = expect(text.find("\"hoverProvider\":true") != std::string::npos, "expected hover capability") && passed;
    passed = expect(text.find("\"label\":\"square\"") != std::string::npos, "expected completion response") && passed;
    passed = expect(text.find("total: int") != std::string::npos, "expected hover response") && passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = diagnoses_valid_source() && passed;
    passed = diagnoses_type_errors_with_range() && passed;
    passed = completes_keywords_and_local_symbols() && passed;
    passed = completes_imported_module_members() && passed;
    passed = hovers_local_symbols() && passed;
    passed = hovers_imported_module_members() && passed;
    passed = publishes_lsp_diagnostics() && passed;
    passed = serves_lsp_completions_and_hover() && passed;
    return passed ? 0 : 1;
}
