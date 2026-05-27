#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace dune::lsp {

struct Diagnostic {
    std::size_t line = 1;
    std::size_t start_column = 1;
    std::size_t end_column = 1;
    std::string message;
};

struct CompletionItem {
    std::string label;
    std::string detail;
    std::size_t kind = 1;
};

struct Hover {
    std::string contents;
};

std::vector<Diagnostic> diagnose_source(const std::string& source, const std::string& uri = {},
                                        const std::filesystem::path& source_directory = {});

std::vector<CompletionItem> complete_source(const std::string& source, const std::string& uri = {},
                                            const std::filesystem::path& source_directory = {}, std::size_t line = 0,
                                            std::size_t character = 0);

std::optional<Hover> hover_source(const std::string& source, const std::string& uri = {},
                                  const std::filesystem::path& source_directory = {}, std::size_t line = 0,
                                  std::size_t character = 0);

int run(std::istream& input, std::ostream& output);

} // namespace dune::lsp
