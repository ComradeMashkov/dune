#pragma once

#include "diagnostics/source_location.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dune {

enum class RuntimeErrorKind {
    panic,
    bounds,
    arithmetic,
    type,
    io,
    foreign,
    internal,
};

inline std::string_view runtime_error_kind_name(RuntimeErrorKind kind) {
    switch (kind) {
    case RuntimeErrorKind::panic:
        return "panic";
    case RuntimeErrorKind::bounds:
        return "bounds error";
    case RuntimeErrorKind::arithmetic:
        return "arithmetic error";
    case RuntimeErrorKind::type:
        return "runtime type error";
    case RuntimeErrorKind::io:
        return "I/O error";
    case RuntimeErrorKind::foreign:
        return "foreign function error";
    case RuntimeErrorKind::internal:
        return "VM error";
    }
    return "runtime error";
}

struct RuntimeStackFrame {
    std::string function;
    SourceLocation location;
};

struct RuntimeFailure {
    RuntimeErrorKind kind = RuntimeErrorKind::internal;
    std::string message;
    std::vector<RuntimeStackFrame> frames;
};

inline void append_stack_trace(std::ostringstream& output, const std::vector<RuntimeStackFrame>& frames) {
    if (frames.empty()) {
        return;
    }
    output << "\nstack trace:";
    for (std::size_t index = 0; index < frames.size(); ++index) {
        const RuntimeStackFrame& frame = frames[index];
        output << "\n  " << index << ": " << frame.function;
        if (!frame.location.source_name.empty()) {
            output << "\n      at " << frame.location.source_name << ':' << frame.location.line << ':'
                   << frame.location.column;
        }
    }
}

inline std::string format_runtime_error(RuntimeErrorKind kind, std::string_view message,
                                        const std::vector<RuntimeStackFrame>& frames,
                                        const std::vector<RuntimeFailure>& cleanup_errors = {}) {
    std::ostringstream output;
    output << runtime_error_kind_name(kind) << ": " << message;
    append_stack_trace(output, frames);
    for (const RuntimeFailure& cleanup_error : cleanup_errors) {
        output << "\nwhile running deferred cleanup: " << cleanup_error.message;
        append_stack_trace(output, cleanup_error.frames);
    }
    return output.str();
}

class RuntimeError final : public std::runtime_error {
public:
    RuntimeError(RuntimeErrorKind kind, std::string message, std::vector<RuntimeStackFrame> frames = {},
                 std::vector<RuntimeFailure> cleanup_errors = {})
        : std::runtime_error(format_runtime_error(kind, message, frames, cleanup_errors)), kind_(kind),
          message_(std::move(message)), frames_(std::move(frames)), cleanup_errors_(std::move(cleanup_errors)) {}

    RuntimeErrorKind kind() const noexcept {
        return kind_;
    }

    const std::string& message() const noexcept {
        return message_;
    }

    const std::vector<RuntimeStackFrame>& frames() const noexcept {
        return frames_;
    }

    const std::vector<RuntimeFailure>& cleanup_errors() const noexcept {
        return cleanup_errors_;
    }

private:
    RuntimeErrorKind kind_;
    std::string message_;
    std::vector<RuntimeStackFrame> frames_;
    std::vector<RuntimeFailure> cleanup_errors_;
};

} // namespace dune
