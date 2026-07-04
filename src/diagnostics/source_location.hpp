#pragma once

#include <cstddef>

namespace dune {

// A single-line source anchor: a 1-based line and column, plus the character
// length of the token/span it points at. It intentionally cannot cross a
// newline — multi-line spans are a follow-up (see issue #41).
struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t length = 1;
};

} // namespace dune
