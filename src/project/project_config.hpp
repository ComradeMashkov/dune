#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dune {

struct ProjectConfig {
    std::filesystem::path root;
    std::filesystem::path manifest_path;
    std::string name;
    std::string version;
    std::vector<std::filesystem::path> sources;
    std::vector<std::filesystem::path> tests;

    std::vector<std::filesystem::path> module_roots() const;
};

std::optional<std::filesystem::path> find_project_root(const std::filesystem::path& start);
std::optional<ProjectConfig> load_project_config(const std::filesystem::path& start);
std::vector<std::filesystem::path> project_module_roots_for(const std::filesystem::path& start);

} // namespace dune
