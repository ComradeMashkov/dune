#pragma once

#include "diagnostics/diagnostic.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <string_view>

namespace dune {

inline std::string_view severity_label(Severity severity) {
    switch (severity) {
    case Severity::error:
        return "error";
    case Severity::warning:
        return "warning";
    case Severity::note:
        return "note";
    }

    return "error";
}

// Copies the 1-based `line` out of `source` (without its trailing newline, and
// tolerating CRLF) into `out`. Returns false when the line is out of range.
inline bool extract_source_line(std::string_view source, std::size_t line, std::string_view& out) {
    if (line == 0) {
        return false;
    }

    std::size_t current = 1;
    std::size_t start = 0;
    for (std::size_t index = 0; index <= source.size(); ++index) {
        if (index == source.size() || source[index] == '\n') {
            if (current == line) {
                std::size_t end = index;
                if (end > start && source[end - 1] == '\r') {
                    --end;
                }
                out = source.substr(start, end - start);
                return true;
            }
            ++current;
            start = index + 1;
        }
    }

    return false;
}

// Renders a Rust-style source snippet for a located diagnostic against `source`:
//
//   error: <message>
//     --> <file>:<line>:<col>
//      |
//    N | <source line>
//      |          ^^^^^^^
//
// Returns an empty string when the diagnostic has no location or its line falls
// outside `source`, so the caller can fall back to the plain message form.
inline std::string render_snippet(const Diagnostic& diagnostic, std::string_view source, std::string_view filename) {
    if (!diagnostic.has_location) {
        return {};
    }

    std::string_view line_text;
    if (!extract_source_line(source, diagnostic.location.line, line_text)) {
        return {};
    }

    const std::size_t column = diagnostic.location.column == 0 ? 1 : diagnostic.location.column;
    const std::size_t indent = column - 1;

    std::size_t carets = diagnostic.location.length == 0 ? 1 : diagnostic.location.length;
    if (indent < line_text.size()) {
        carets = std::min(carets, line_text.size() - indent);
    } else {
        carets = 1;
    }
    carets = std::max<std::size_t>(carets, 1);

    const std::string number = std::to_string(diagnostic.location.line);
    const std::string gutter(number.size(), ' ');

    std::string out;
    out += std::string(severity_label(diagnostic.severity)) + ": " + diagnostic.message + "\n";
    out += gutter + " --> " + std::string(filename) + ":" + number + ":" + std::to_string(column) + "\n";
    out += gutter + " |\n";
    out += number + " | " + std::string(line_text) + "\n";
    out += gutter + " | " + std::string(indent, ' ') + std::string(carets, '^') + "\n";
    return out;
}

// The rendered body the CLI prints for a failed step: a source snippet when
// `error` is a located `DiagnosticError` that lands inside `source`, otherwise
// the legacy 10-space-indented message line. `allow_snippet` lets callers
// suppress snippets for stages whose locations refer to another file (e.g.
// module resolution), where `source` is the wrong file to quote.
inline std::string render_error_body(const std::exception& error, std::string_view source, std::string_view filename,
                                     bool allow_snippet) {
    if (allow_snippet) {
        if (const auto* located = dynamic_cast<const DiagnosticError*>(&error)) {
            const std::string snippet = render_snippet(located->diagnostic(), source, filename);
            if (!snippet.empty()) {
                return snippet;
            }
        }
    }

    return "          " + std::string(error.what()) + "\n";
}

} // namespace dune
