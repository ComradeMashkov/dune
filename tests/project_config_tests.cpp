#include "project/project_config.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

namespace fs = std::filesystem;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

void write_file(const fs::path& path, const std::string& content) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not write " + path.string());
    }
    output << content;
}

fs::path fresh_root(const std::string& name) {
    const fs::path root = fs::current_path() / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

bool loads_manifest_from_source_file() {
    const fs::path root = fresh_root("project_config_loads_manifest");
    fs::create_directories(root / "src" / "app");
    fs::create_directories(root / "spec");
    write_file(root / "dune.toml", "name = \"demo\"\n"
                                   "version = \"0.1.0\"\n"
                                   "sources = [\"src\", \"lib\"]\n"
                                   "tests = [\"spec\"]\n");

    const std::optional<dune::ProjectConfig> config = dune::load_project_config(root / "src" / "app" / "main.dn");
    bool passed = expect(config.has_value(), "expected project config") &&
                  expect(config->root == root.lexically_normal(), "expected discovered project root") &&
                  expect(config->name == "demo", "expected project name") &&
                  expect(config->version == "0.1.0", "expected project version") &&
                  expect(config->sources.size() == 2 && config->sources[0] == "src" && config->sources[1] == "lib",
                         "expected configured source roots") &&
                  expect(config->tests.size() == 1 && config->tests[0] == "spec", "expected configured test root");

    const std::vector<fs::path> roots = config->module_roots();
    passed = expect(roots.size() == 3, "expected source and test module roots") && passed;
    passed = expect(roots[0] == (root / "src").lexically_normal(), "expected first source root") && passed;
    passed = expect(roots[1] == (root / "lib").lexically_normal(), "expected second source root") && passed;
    passed = expect(roots[2] == (root / "spec").lexically_normal(), "expected test root") && passed;
    return passed;
}

bool defaults_to_conventional_roots() {
    const fs::path root = fresh_root("project_config_defaults");
    write_file(root / "dune.toml", "name = \"demo\"\n");

    const std::optional<dune::ProjectConfig> config = dune::load_project_config(root);
    bool passed = expect(config.has_value(), "expected config with defaults") &&
                  expect(config->sources.size() == 1 && config->sources[0] == "src", "expected default src root") &&
                  expect(config->tests.size() == 1 && config->tests[0] == "tests", "expected default tests root");

    const std::vector<fs::path> roots = dune::project_module_roots_for(root / "src" / "main.dn");
    passed = expect(roots.size() == 2, "expected default module roots") && passed;
    return passed;
}

bool returns_empty_without_manifest() {
    const fs::path root = fresh_root("project_config_no_manifest");
    const std::optional<fs::path> project_root = dune::find_project_root(root / "src" / "main.dn");
    return expect(!project_root.has_value(), "expected no project root without dune.toml") &&
           expect(dune::project_module_roots_for(root).empty(), "expected no module roots without manifest");
}

bool rejects_parent_paths() {
    const fs::path root = fresh_root("project_config_rejects_parent");
    write_file(root / "dune.toml", "sources = [\"../outside\"]\n");

    try {
        (void)dune::load_project_config(root);
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        return expect(message.find("project paths must be non-empty relative paths") != std::string::npos,
                      "expected unsafe path diagnostic");
    }

    std::cerr << "expected invalid manifest to fail\n";
    return false;
}

} // namespace

int main() {
    bool passed = true;
    passed = loads_manifest_from_source_file() && passed;
    passed = defaults_to_conventional_roots() && passed;
    passed = returns_empty_without_manifest() && passed;
    passed = rejects_parent_paths() && passed;
    return passed ? 0 : 1;
}
