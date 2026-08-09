#pragma once

#include <cstddef>
#include <string>

namespace dune {

// A single-line source anchor: a 1-based line and column, plus the character
// length of the token/span it points at. It intentionally cannot cross a
// newline — multi-line spans are a follow-up (see issue #41).
struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t length = 1;
    // Empty for synthetic/unknown locations. The module loader fills this for
    // user files, imported modules, REPL entries, and notebook cells before the
    // compiler lowers the AST to source-mapped bytecode.
    std::string source_name;
};

} // namespace dune
