#include "diagnostics/diagnostic.hpp"
#include "diagnostics/snippet.hpp"

#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }

    return true;
}

bool expect_eq(const std::string& actual, const std::string& expected, const char* message) {
    if (actual != expected) {
        std::cerr << message << ": expected\n[" << expected << "]\ngot\n[" << actual << "]\n";
        return false;
    }

    return true;
}

// The legacy `"line L, columns A-B: msg"` wire format must stay byte-stable: the
// LSP and every error-substring test depend on it.
bool formats_legacy_prefix() {
    const std::string formatted = dune::format_diagnostic(dune::SourceLocation{4, 12, 5}, "boom");
    return expect_eq(formatted, "line 4, columns 12-16: boom", "format_diagnostic prefix");
}

bool located_error_exposes_structure_and_legacy_what() {
    const dune::DiagnosticError error(dune::SourceLocation{1, 10, 7}, "expected type 'int' but got 'text'");
    bool passed = true;
    passed = expect(error.diagnostic().has_location, "located error has_location") && passed;
    passed = expect(error.diagnostic().location.line == 1, "located error line") && passed;
    passed = expect(error.diagnostic().location.column == 10, "located error column") && passed;
    passed = expect(error.diagnostic().location.length == 7, "located error length") && passed;
    passed =
        expect(error.diagnostic().message == "expected type 'int' but got 'text'", "located error message") && passed;
    passed =
        expect_eq(error.what(), "line 1, columns 10-16: expected type 'int' but got 'text'", "located error what()") &&
        passed;
    return passed;
}

bool message_only_error_has_no_location() {
    const dune::DiagnosticError error("unknown string escape");
    bool passed = true;
    passed = expect(!error.diagnostic().has_location, "message-only error has no location") && passed;
    passed = expect_eq(error.what(), "unknown string escape", "message-only error what()") && passed;
    return passed;
}

bool extracts_source_lines() {
    const std::string source = "first\nsecond\r\nthird";
    std::string_view line;
    bool passed = true;
    passed = expect(dune::extract_source_line(source, 1, line) && line == "first", "line 1") && passed;
    passed = expect(dune::extract_source_line(source, 2, line) && line == "second", "line 2 strips CR") && passed;
    passed = expect(dune::extract_source_line(source, 3, line) && line == "third", "line 3 (no newline)") && passed;
    passed = expect(!dune::extract_source_line(source, 4, line), "line 4 out of range") && passed;
    passed = expect(!dune::extract_source_line(source, 0, line), "line 0 rejected") && passed;
    return passed;
}

bool renders_rust_style_snippet() {
    const dune::Diagnostic diagnostic{dune::Severity::error, dune::SourceLocation{1, 10, 7},
                                      "expected type 'int' but got 'text'", true};
    const std::string rendered = dune::render_snippet(diagnostic, "x: int = \"hello\";", "main.dn");
    const std::string expected = "error: expected type 'int' but got 'text'\n"
                                 "  --> main.dn:1:10\n"
                                 "  |\n"
                                 "1 | x: int = \"hello\";\n"
                                 "  |          ^^^^^^^\n";
    return expect_eq(rendered, expected, "render_snippet");
}

bool snippet_uses_line_number_gutter() {
    const dune::Diagnostic diagnostic{dune::Severity::error, dune::SourceLocation{12, 1, 3}, "bad", true};
    const std::string rendered = dune::render_snippet(diagnostic, "\n\n\n\n\n\n\n\n\n\n\n abc", "f.dn");
    // The gutter widens to the line-number width and the separators stay aligned.
    const std::string expected = "error: bad\n"
                                 "   --> f.dn:12:1\n"
                                 "   |\n"
                                 "12 |  abc\n"
                                 "   | ^^^\n";
    return expect_eq(rendered, expected, "render_snippet gutter width");
}

bool snippet_empty_when_unrenderable() {
    bool passed = true;
    const dune::Diagnostic unlocated{dune::Severity::error, {}, "no span", false};
    passed = expect(dune::render_snippet(unlocated, "code", "f.dn").empty(), "unlocated -> empty") && passed;
    const dune::Diagnostic out_of_range{dune::Severity::error, dune::SourceLocation{9, 1, 1}, "oops", true};
    passed =
        expect(dune::render_snippet(out_of_range, "only one line", "f.dn").empty(), "out-of-range -> empty") && passed;
    return passed;
}

bool error_body_prefers_snippet_then_falls_back() {
    bool passed = true;
    const dune::DiagnosticError located(dune::SourceLocation{1, 1, 4}, "bad token");
    const std::string snippet = dune::render_error_body(located, "code here", "f.dn", true);
    passed = expect(snippet.rfind("error: bad token\n", 0) == 0, "body renders snippet for located error") && passed;

    // Suppressed snippet (e.g. module stage) falls back to the indented message.
    const std::string suppressed = dune::render_error_body(located, "code here", "f.dn", false);
    passed = expect_eq(suppressed, "          line 1, columns 1-4: bad token\n", "body falls back when suppressed") &&
             passed;

    // A plain exception always falls back to its message.
    const std::runtime_error plain("plain failure");
    passed = expect_eq(dune::render_error_body(plain, "code", "f.dn", true), "          plain failure\n",
                       "body falls back for non-diagnostic") &&
             passed;
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = formats_legacy_prefix() && passed;
    passed = located_error_exposes_structure_and_legacy_what() && passed;
    passed = message_only_error_has_no_location() && passed;
    passed = extracts_source_lines() && passed;
    passed = renders_rust_style_snippet() && passed;
    passed = snippet_uses_line_number_gutter() && passed;
    passed = snippet_empty_when_unrenderable() && passed;
    passed = error_body_prefers_snippet_then_falls_back() && passed;
    return passed ? 0 : 1;
}
