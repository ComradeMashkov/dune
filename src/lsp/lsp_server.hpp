#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace dune::lsp {

struct Diagnostic {
    std::size_t line = 1;
    std::size_t start_column = 1;
    std::size_t end_column = 1;
    std::string message;
};

std::vector<Diagnostic> diagnose_source(const std::string& source, const std::string& uri = {},
                                        const std::filesystem::path& source_directory = {});

int run(std::istream& input, std::ostream& output);

} // namespace dune::lsp
