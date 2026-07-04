#pragma once

#include <string>

namespace dune {

struct NativeCanvasDisplayResult {
    bool ok = false;
    std::string message;
};

// Open a native window showing deterministic SVG (a plot chart or a canvas
// scene). `title` names the window; an empty title falls back to a default.
NativeCanvasDisplayResult show_native_canvas_svg(const std::string& title, const std::string& svg);

} // namespace dune
