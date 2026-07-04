#include "native_canvas_display.hpp"

namespace dune {

NativeCanvasDisplayResult show_native_canvas_svg(const std::string& title, const std::string& svg) {
    (void)title;
    (void)svg;
    return {false, "native canvas display backend is not available on this platform yet"};
}

} // namespace dune
