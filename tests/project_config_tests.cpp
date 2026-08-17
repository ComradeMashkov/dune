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
    if (!config.has_value()) {
        expect(false, "expected project config");
        return false;
    }
    const dune::ProjectConfig& project = config.value();
    bool passed = expect(project.root == root.lexically_normal(), "expected discovered project root") &&
                  expect(project.name == "demo", "expected project name") &&
                  expect(project.version == "0.1.0", "expected project version") &&
                  expect(project.sources.size() == 2 && project.sources[0] == "src" && project.sources[1] == "lib",
                         "expected configured source roots") &&
                  expect(project.tests.size() == 1 && project.tests[0] == "spec", "expected configured test root");

    const std::vector<fs::path> roots = project.module_roots();
    passed = expect(roots.size() == 3, "expected source and test module roots") && passed;
    passed = expect(roots[0] == (root / "src").lexically_normal(), "expected first source root") && passed;
    passed = expect(roots[1] == (root / "lib").lexically_normal(), "expected second source root") && passed;
    passed = expect(roots[2] == (root / "spec").lexically_normal(), "expected test root") && passed;

    const std::vector<fs::path> application_roots = project.module_roots_for(root / "src" / "app" / "main.dn");
    passed = expect(application_roots.size() == 2, "expected application code to exclude test roots") && passed;
    const std::vector<fs::path> test_roots = project.module_roots_for(root / "spec" / "feature.dn");
    passed = expect(test_roots.size() == 3, "expected test code to include source and test roots") && passed;
    return passed;
}

bool defaults_to_conventional_roots() {
    const fs::path root = fresh_root("project_config_defaults");
    write_file(root / "dune.toml", "name = \"demo\"\n");

    const std::optional<dune::ProjectConfig> config = dune::load_project_config(root);
    if (!config.has_value()) {
        expect(false, "expected config with defaults");
        return false;
    }
    const dune::ProjectConfig& project = config.value();
    bool passed = expect(project.sources.size() == 1 && project.sources[0] == "src", "expected default src root") &&
                  expect(project.tests.size() == 1 && project.tests[0] == "tests", "expected default tests root");

    const std::vector<fs::path> source_roots = dune::project_module_roots_for(root / "src" / "main.dn");
    passed = expect(source_roots.size() == 1, "expected source code to use only the default source root") && passed;
    const std::vector<fs::path> test_roots = dune::project_module_roots_for(root / "tests" / "main.dn");
    passed = expect(test_roots.size() == 2, "expected test code to use source and test roots") && passed;
    return passed;
}

bool supports_comments_trailing_commas_and_empty_tests() {
    const fs::path root = fresh_root("project_config_toml_subset");
    write_file(root / "dune.toml", "\xef\xbb\xbfname = \"demo#tools\" # package comment\n"
                                   "version = \"1.2.3\"\n"
                                   "sources = [\".\", \"lib\",] # roots\n"
                                   "tests = []\n");

    const std::optional<dune::ProjectConfig> config = dune::load_project_config(root);
    if (!config.has_value()) {
        expect(false, "expected manifest with comments and trailing comma");
        return false;
    }
    const dune::ProjectConfig& project = config.value();
    bool passed = expect(project.name == "demo#tools", "expected hash inside string to be preserved") &&
                  expect(project.sources.size() == 2, "expected both configured source roots") &&
                  expect(project.tests.empty(), "expected explicit empty test roots to remain empty");
    passed = expect(project.module_roots_for(root / "main.dn").size() == 2,
                    "expected no implicit test root after tests = []") &&
             passed;
    return passed;
}

bool nearest_manifest_wins() {
    const fs::path root = fresh_root("project_config_nested");
    fs::create_directories(root / "packages" / "inner" / "code");
    write_file(root / "dune.toml", "name = \"outer\"\n");
    write_file(root / "packages" / "inner" / "dune.toml", "name = \"inner\"\nsources = [\"code\"]\n");

    const std::optional<dune::ProjectConfig> config =
        dune::load_project_config(root / "packages" / "inner" / "code" / "main.dn");
    if (!config.has_value()) {
        expect(false, "expected nested project config");
        return false;
    }
    const dune::ProjectConfig& project = config.value();
    return expect(project.name == "inner", "expected nearest nested manifest") &&
           expect(project.root == (root / "packages" / "inner").lexically_normal(), "expected nested project root");
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

bool rejects_symlink_escapes_when_supported() {
    const fs::path root = fresh_root("project_config_rejects_symlink_escape");
    const fs::path outside = fresh_root("project_config_symlink_escape_target");
    std::error_code error;
    fs::create_directory_symlink(outside, root / "linked", error);
    if (error) {
        fs::remove_all(outside);
        return true;
    }
    write_file(root / "dune.toml", "name = \"a\"\nsources = [\"linked\"]\n");

    try {
        (void)dune::load_project_config(root);
    } catch (const std::runtime_error& exception) {
        fs::remove_all(outside);
        return expect(std::string(exception.what()).find("resolves outside the project root") != std::string::npos,
                      "expected symlink escape diagnostic");
    }

    fs::remove_all(outside);
    std::cerr << "expected escaping symlink root to fail\n";
    return false;
}

bool rejects_invalid_manifests() {
    const auto rejects = [](const std::string& directory, const std::string& manifest, const std::string& expected) {
        const fs::path root = fresh_root(directory);
        write_file(root / "dune.toml", manifest);
        try {
            (void)dune::load_project_config(root);
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            return expect(message.find(expected) != std::string::npos, "expected precise manifest diagnostic") &&
                   expect(message.find("dune.toml:") != std::string::npos,
                          "expected manifest path and line in diagnostic");
        }
        std::cerr << "expected invalid manifest to fail: " << directory << '\n';
        return false;
    };

    bool passed = true;
    passed = rejects("project_config_missing_name", "sources = [\"src\"]\n", "missing required") && passed;
    passed = rejects("project_config_duplicate_key", "name = \"a\"\nname = \"b\"\n", "duplicate project") && passed;
    passed = rejects("project_config_empty_sources", "name = \"a\"\nsources = []\n", "at least one") && passed;
    passed = rejects("project_config_duplicate_path", "name = \"a\"\nsources = [\"src\", \"src\"]\n",
                     "duplicate project path") &&
             passed;
    passed = rejects("project_config_empty_array_item", "name = \"a\"\nsources = [\"src\",,\"lib\"]\n",
                     "between array commas") &&
             passed;
    passed =
        rejects("project_config_unknown_key", "name = \"a\"\ntarget = \"main\"\n", "unsupported project") && passed;
    {
        const fs::path root = fresh_root("project_config_non_directory_root");
        write_file(root / "source.dn", "");
        write_file(root / "dune.toml", "name = \"a\"\nsources = [\"source.dn\"]\n");
        try {
            (void)dune::load_project_config(root);
            passed = expect(false, "expected a file module root to fail") && passed;
        } catch (const std::runtime_error& error) {
            passed = expect(std::string(error.what()).find("must be a directory") != std::string::npos,
                            "expected non-directory source-root diagnostic") &&
                     passed;
        }
    }
    {
        const fs::path root = fresh_root("project_config_non_directory_default_root");
        write_file(root / "src", "");
        write_file(root / "dune.toml", "name = \"a\"\n");
        try {
            (void)dune::load_project_config(root);
            passed = expect(false, "expected a default source root that is a file to fail") && passed;
        } catch (const std::runtime_error& error) {
            passed = expect(std::string(error.what()).find("must be a directory") != std::string::npos,
                            "expected non-directory default source-root diagnostic") &&
                     passed;
        }
    }
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = loads_manifest_from_source_file() && passed;
    passed = defaults_to_conventional_roots() && passed;
    passed = supports_comments_trailing_commas_and_empty_tests() && passed;
    passed = nearest_manifest_wins() && passed;
    passed = returns_empty_without_manifest() && passed;
    passed = rejects_parent_paths() && passed;
    passed = rejects_symlink_escapes_when_supported() && passed;
    passed = rejects_invalid_manifests() && passed;
    return passed ? 0 : 1;
}
