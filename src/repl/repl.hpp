#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>

namespace dune::repl {

struct Options {
    std::string version;
    std::filesystem::path source_directory;
    bool show_prompts = false;
};

int run(std::istream& input, std::ostream& output, std::ostream& error, const Options& options);

} // namespace dune::repl
