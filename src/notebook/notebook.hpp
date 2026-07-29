#pragma once

#include "repl/repl.hpp"

#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dune::notebook {

enum class CellKind {
    markdown,
    code,
};

struct Cell {
    std::string id;
    CellKind kind = CellKind::markdown;
    std::string source;
    std::string output;
    std::string error;
    std::size_t execution_count = 0;
};

struct Document {
    std::size_t format_version = 1;
    std::string title;
    std::filesystem::path path;
    std::vector<Cell> cells;
};

struct CellExecution {
    std::size_t cell_index = 0;
    std::size_t execution_count = 0;
    bool success = false;
    std::string output;
    std::string error;
};

struct ExecutionReport {
    bool success = true;
    std::size_t failed_cell = std::numeric_limits<std::size_t>::max();
    std::vector<CellExecution> cells;
};

Document parse(std::string_view json, std::filesystem::path path = {});
std::string serialize(const Document& document);
Document read(const std::filesystem::path& path);
void write(const Document& document, const std::filesystem::path& path);
std::string render_html(const Document& document);
void apply_execution(Document& document, const ExecutionReport& report);

class Kernel {
public:
    explicit Kernel(std::filesystem::path source_directory);

    ExecutionReport execute(const Document& document, std::optional<std::size_t> through_cell = std::nullopt);
    void reset();

private:
    void rebuild_session();

    std::filesystem::path source_directory_;
    std::unique_ptr<repl::Session> session_;
    std::vector<std::string> executed_ids_;
    std::vector<std::string> executed_sources_;
    std::vector<CellExecution> executions_;
    std::size_t execution_count_ = 0;
};

} // namespace dune::notebook
