#include "native_canvas_display.hpp"

namespace dune {

// Portable fallback for platforms without a native windowing backend. The macOS
// implementation lives in native_canvas_display_macos.mm.
NativeCanvasDisplayResult show_native_canvas_svg(const std::string& title, const std::string& svg) {
    (void)title;
    (void)svg;
    return {false, "native canvas display backend is not available on this platform yet"};
}

} // namespace dune
