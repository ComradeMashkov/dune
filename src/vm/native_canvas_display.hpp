#pragma once

#include <string>

namespace dune {

struct NativeCanvasDisplayResult {
    bool ok = false;
    std::string message;
};

NativeCanvasDisplayResult show_native_canvas_svg(const std::string& title, const std::string& svg);

} // namespace dune
