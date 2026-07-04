#include "native_plot_display.hpp"

namespace dune {

NativePlotDisplayResult show_native_plot_svg(const std::string& svg) {
    (void)svg;
    return {false, "native plot display backend is not available on this platform yet"};
}

} // namespace dune
