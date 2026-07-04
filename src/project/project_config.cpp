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

    std::vector<std::string> items;
    std::string current;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t index = 1; index + 1 < trimmed.size(); ++index) {
        const char character = trimmed[index];
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

    if (!trim(current).empty()) {
        items.push_back(trim(current));
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

std::vector<std::filesystem::path> parse_path_array(const std::filesystem::path& path, std::size_t line,
                                                    const std::string& value) {
    std::vector<std::filesystem::path> result;
    for (const std::string& item : split_array_items(path, line, value)) {
        const std::string text = parse_string(path, line, item);
        const std::filesystem::path root = text;
        if (root.empty() || root.is_absolute() || has_parent_segment(root)) {
            manifest_error(path, line, "project paths must be non-empty relative paths inside the project");
        }
        result.push_back(root.lexically_normal());
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

    std::istringstream input(read_text_file(manifest_path));
    std::string line_text;
    for (std::size_t line = 1; std::getline(input, line_text); ++line) {
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
        if (key == "name") {
            config.name = parse_string(manifest_path, line, value);
        } else if (key == "version") {
            config.version = parse_string(manifest_path, line, value);
        } else if (key == "sources") {
            config.sources = parse_path_array(manifest_path, line, value);
        } else if (key == "tests") {
            config.tests = parse_path_array(manifest_path, line, value);
        } else {
            manifest_error(manifest_path, line, "unsupported project manifest key '" + key + "'");
        }
    }

    if (config.sources.empty()) {
        config.sources.push_back("src");
    }
    if (config.tests.empty()) {
        config.tests.push_back("tests");
    }

    return config;
}

} // namespace

std::vector<std::filesystem::path> ProjectConfig::module_roots() const {
    std::vector<std::filesystem::path> roots;
    for (const std::filesystem::path& source : sources) {
        add_unique_path(roots, root / source);
    }
    for (const std::filesystem::path& test : tests) {
        add_unique_path(roots, root / test);
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

    return project->module_roots();
}

} // namespace dune
