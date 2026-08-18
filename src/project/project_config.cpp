#include "project_config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

namespace dune {

namespace {

constexpr const char* kManifestName = "dune.toml";

std::filesystem::path absolute_normal(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path absolute = std::filesystem::absolute(path, error);
    if (error) {
        absolute = path;
    }
    return absolute.lexically_normal();
}

std::filesystem::path canonical_normal(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    return error ? absolute_normal(path) : canonical.lexically_normal();
}

bool path_is_within(const std::filesystem::path& path, const std::filesystem::path& directory) {
    const std::filesystem::path normalized_path = canonical_normal(path);
    const std::filesystem::path normalized_directory = canonical_normal(directory);
    const auto [directory_end, path_position] = std::mismatch(normalized_directory.begin(), normalized_directory.end(),
                                                              normalized_path.begin(), normalized_path.end());
    (void)path_position;
    return directory_end == normalized_directory.end();
}

std::filesystem::path directory_start(const std::filesystem::path& start) {
    if (start.empty()) {
        return absolute_normal(std::filesystem::current_path());
    }

    std::error_code error;
    if (std::filesystem::is_regular_file(start, error)) {
        return absolute_normal(start.parent_path());
    }

    if (!std::filesystem::exists(start, error) && start.has_extension()) {
        return absolute_normal(start.parent_path());
    }

    return absolute_normal(start);
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open project manifest '" + path.string() + "'");
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string trim(std::string value) {
    auto is_space = [](unsigned char character) { return std::isspace(character) != 0; };
    value.erase(value.begin(), std::ranges::find_if_not(value, is_space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
    return value;
}

std::string strip_comment(std::string_view line) {
    std::string result;
    bool in_string = false;
    bool escaped = false;
    for (const char character : line) {
        if (escaped) {
            result += character;
            escaped = false;
            continue;
        }

        if (character == '\\' && in_string) {
            result += character;
            escaped = true;
            continue;
        }

        if (character == '"') {
            in_string = !in_string;
            result += character;
            continue;
        }

        if (character == '#' && !in_string) {
            break;
        }

        result += character;
    }

    return result;
}

[[noreturn]] void manifest_error(const std::filesystem::path& path, std::size_t line, const std::string& message) {
    throw std::runtime_error(path.string() + ":" + std::to_string(line) + ": " + message);
}

std::string parse_string(const std::filesystem::path& path, std::size_t line, const std::string& value) {
    if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
        manifest_error(path, line, "expected a quoted string");
    }

    std::string result;
    bool escaped = false;
    for (std::size_t index = 1; index + 1 < value.size(); ++index) {
        const char character = value[index];
        if (escaped) {
            switch (character) {
            case '"':
            case '\\':
                result += character;
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            default:
                manifest_error(path, line, std::string("unsupported string escape '\\") + character + "'");
            }
            escaped = false;
            continue;
        }

        if (character == '\\') {
            escaped = true;
            continue;
        }

        if (character == '"') {
            manifest_error(path, line, "unexpected quote in string");
        }

        result += character;
    }

    if (escaped) {
        manifest_error(path, line, "unterminated string escape");
    }

    return result;
}

std::vector<std::string> split_array_items(const std::filesystem::path& path, std::size_t line,
                                           const std::string& value) {
    const std::string trimmed = trim(value);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
        manifest_error(path, line, "expected an array of quoted strings");
    }

    const std::string contents = trimmed.substr(1, trimmed.size() - 2);
    if (trim(contents).empty()) {
        return {};
    }

    std::vector<std::string> items;
    std::string current;
    bool in_string = false;
    bool escaped = false;
    for (const char character : contents) {
        if (escaped) {
            current += character;
            escaped = false;
            continue;
        }

        if (character == '\\' && in_string) {
            current += character;
            escaped = true;
            continue;
        }

        if (character == '"') {
            in_string = !in_string;
            current += character;
            continue;
        }

        if (character == ',' && !in_string) {
            items.push_back(trim(current));
            current.clear();
            continue;
        }

        current += character;
    }

    if (in_string) {
        manifest_error(path, line, "unterminated string in array");
    }

    items.push_back(trim(current));
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (!items[index].empty()) {
            continue;
        }
        if (index + 1 == items.size()) {
            items.pop_back();
            break;
        }
        manifest_error(path, line, "expected a quoted string between array commas");
    }

    return items;
}

bool has_parent_segment(const std::filesystem::path& path) {
    for (const std::filesystem::path& part : path) {
        if (part == "..") {
            return true;
        }
    }
    return false;
}

std::filesystem::path validate_project_path(const std::filesystem::path& manifest_path,
                                            const std::filesystem::path& project_root, std::size_t line,
                                            const std::string& text) {
    const std::filesystem::path root = text;
    if (root.empty() || root.is_absolute() || has_parent_segment(root)) {
        manifest_error(manifest_path, line, "project paths must be non-empty relative paths inside the project");
    }

    const std::filesystem::path normalized = root.lexically_normal();
    if (!path_is_within(project_root / normalized, project_root)) {
        manifest_error(manifest_path, line, "project path '" + text + "' resolves outside the project root");
    }

    std::error_code error;
    const std::filesystem::path resolved = project_root / normalized;
    const bool exists = std::filesystem::exists(resolved, error);
    if (error) {
        manifest_error(manifest_path, line, "could not inspect project path '" + text + "': " + error.message());
    }
    if (exists && !std::filesystem::is_directory(resolved, error)) {
        if (error) {
            manifest_error(manifest_path, line, "could not inspect project path '" + text + "': " + error.message());
        }
        manifest_error(manifest_path, line, "project path '" + text + "' must be a directory");
    }

    return normalized;
}

std::vector<std::filesystem::path> parse_path_array(const std::filesystem::path& path,
                                                    const std::filesystem::path& project_root, std::size_t line,
                                                    const std::string& value) {
    std::vector<std::filesystem::path> result;
    std::unordered_set<std::string> seen;
    for (const std::string& item : split_array_items(path, line, value)) {
        const std::string text = parse_string(path, line, item);
        const std::filesystem::path normalized = validate_project_path(path, project_root, line, text);
        if (!seen.insert(normalized.generic_string()).second) {
            manifest_error(path, line, "duplicate project path '" + text + "'");
        }
        result.push_back(normalized);
    }

    return result;
}

void add_unique_path(std::vector<std::filesystem::path>& paths, const std::filesystem::path& path) {
    const std::filesystem::path normalized = path.lexically_normal();
    const std::string key = normalized.string();
    const bool exists = std::ranges::any_of(
        paths, [&key](const std::filesystem::path& current) { return current.lexically_normal().string() == key; });
    if (!exists) {
        paths.push_back(normalized);
    }
}

ProjectConfig parse_manifest(const std::filesystem::path& manifest_path) {
    ProjectConfig config;
    config.root = absolute_normal(manifest_path.parent_path());
    config.manifest_path = absolute_normal(manifest_path);
    std::unordered_set<std::string> seen_keys;
    bool sources_configured = false;
    bool tests_configured = false;

    std::istringstream input(read_text_file(manifest_path));
    std::string line_text;
    for (std::size_t line = 1; std::getline(input, line_text); ++line) {
        if (line == 1 && line_text.starts_with("\xef\xbb\xbf")) {
            line_text.erase(0, 3);
        }
        const std::string without_comment = strip_comment(line_text);
        const std::string statement = trim(without_comment);
        if (statement.empty()) {
            continue;
        }

        const std::size_t equals = statement.find('=');
        if (equals == std::string::npos) {
            manifest_error(manifest_path, line, "expected key = value");
        }

        const std::string key = trim(statement.substr(0, equals));
        const std::string value = trim(statement.substr(equals + 1));
        if (key.empty()) {
            manifest_error(manifest_path, line, "expected a manifest key before '='");
        }
        if (value.empty()) {
            manifest_error(manifest_path, line, "expected a value for '" + key + "'");
        }
        if (!seen_keys.insert(key).second) {
            manifest_error(manifest_path, line, "duplicate project manifest key '" + key + "'");
        }

        if (key == "name") {
            config.name = parse_string(manifest_path, line, value);
            if (config.name.empty()) {
                manifest_error(manifest_path, line, "project name cannot be empty");
            }
        } else if (key == "version") {
            config.version = parse_string(manifest_path, line, value);
            if (config.version.empty()) {
                manifest_error(manifest_path, line, "project version cannot be empty");
            }
        } else if (key == "sources") {
            sources_configured = true;
            config.sources = parse_path_array(manifest_path, config.root, line, value);
            if (config.sources.empty()) {
                manifest_error(manifest_path, line, "project 'sources' must contain at least one directory");
            }
        } else if (key == "tests") {
            tests_configured = true;
            config.tests = parse_path_array(manifest_path, config.root, line, value);
        } else {
            manifest_error(manifest_path, line, "unsupported project manifest key '" + key + "'");
        }
    }

    if (!seen_keys.contains("name")) {
        manifest_error(manifest_path, 1, "missing required project manifest key 'name'");
    }
    if (!sources_configured) {
        config.sources.push_back(validate_project_path(manifest_path, config.root, 1, "src"));
    }
    if (!tests_configured) {
        config.tests.push_back(validate_project_path(manifest_path, config.root, 1, "tests"));
    }

    return config;
}

} // namespace

std::vector<std::filesystem::path> ProjectConfig::source_roots() const {
    std::vector<std::filesystem::path> roots;
    for (const std::filesystem::path& source : sources) {
        add_unique_path(roots, root / source);
    }
    return roots;
}

std::vector<std::filesystem::path> ProjectConfig::test_roots() const {
    std::vector<std::filesystem::path> roots;
    for (const std::filesystem::path& test : tests) {
        add_unique_path(roots, root / test);
    }
    return roots;
}

std::vector<std::filesystem::path> ProjectConfig::module_roots() const {
    std::vector<std::filesystem::path> roots = source_roots();
    for (const std::filesystem::path& test : test_roots()) {
        add_unique_path(roots, test);
    }
    return roots;
}

std::vector<std::filesystem::path> ProjectConfig::module_roots_for(const std::filesystem::path& source) const {
    std::vector<std::filesystem::path> roots = source_roots();
    const std::filesystem::path location = directory_start(source);
    const std::vector<std::filesystem::path> configured_test_roots = test_roots();
    const bool is_test_source = std::ranges::any_of(
        configured_test_roots, [&](const std::filesystem::path& test) { return path_is_within(location, test); });
    if (is_test_source) {
        for (const std::filesystem::path& test : configured_test_roots) {
            add_unique_path(roots, test);
        }
    }
    return roots;
}

std::optional<std::filesystem::path> find_project_root(const std::filesystem::path& start) {
    std::filesystem::path directory = directory_start(start);
    while (!directory.empty()) {
        const std::filesystem::path manifest = directory / kManifestName;
        std::error_code error;
        if (std::filesystem::is_regular_file(manifest, error)) {
            return directory;
        }

        const std::filesystem::path parent = directory.parent_path();
        if (parent.empty() || parent == directory) {
            break;
        }
        directory = parent;
    }

    return std::nullopt;
}

std::optional<ProjectConfig> load_project_config(const std::filesystem::path& start) {
    const std::optional<std::filesystem::path> root = find_project_root(start);
    if (!root.has_value()) {
        return std::nullopt;
    }

    return parse_manifest(*root / kManifestName);
}

std::vector<std::filesystem::path> project_module_roots_for(const std::filesystem::path& start) {
    const std::optional<ProjectConfig> project = load_project_config(start);
    if (!project.has_value()) {
        return {};
    }

    return project->module_roots_for(start);
}

} // namespace dune
