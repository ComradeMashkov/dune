#pragma once

#include "diagnostics/source_location.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace dune {

enum class Severity {
    error,
    warning,
    note,
};

// A structured compiler diagnostic: a severity, an optional source anchor, and
// a human-readable message. `has_location` is false for errors that cannot yet
// be placed in the source (e.g. some module-resolution failures).
struct Diagnostic {
    Severity severity = Severity::error;
    SourceLocation location;
    std::string message;
    bool has_location = false;
};

// The canonical text form of a located diagnostic. This exact string is what
// `DiagnosticError::what()` returns and what the LSP historically parsed back
// into a range, so its shape is load-bearing: keep it byte-for-byte stable.
inline std::string format_diagnostic(const SourceLocation& location, const std::string& message) {
    return "line " + std::to_string(location.line) + ", columns " + std::to_string(location.column) + "-" +
           std::to_string(location.column + location.length - 1) + ": " + message;
}

// A front-end error (lexer/parser/type-checker/module-loader) carrying a
// structured `Diagnostic`. `what()` stays the legacy `"line L, columns A-B: msg"`
// string (or the bare message when unlocated) so existing substring tests and
// the LSP's string fallback keep working, while consumers that understand the
// type can read the structured location via `diagnostic()`.
class DiagnosticError : public std::runtime_error {
public:
    DiagnosticError(SourceLocation location, std::string message)
        : std::runtime_error(format_diagnostic(location, message)),
          diagnostic_{Severity::error, location, std::move(message), true} {}

    explicit DiagnosticError(std::string message)
        : std::runtime_error(message), diagnostic_{Severity::error, {}, std::move(message), false} {}

    const Diagnostic& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    Diagnostic diagnostic_;
};

} // namespace dune
