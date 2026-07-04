#pragma once

#include <string>

namespace dune {

struct NativePlotDisplayResult {
    bool ok = false;
    std::string message;
};

NativePlotDisplayResult show_native_plot_svg(const std::string& svg);

} // namespace dune
