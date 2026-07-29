#include "cli.hpp"

#include "notebook/notebook.hpp"
#include "notebook/server.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace dune::notebook {

namespace {

void print_report(const ExecutionReport& report, std::ostream& output, std::ostream& error) {
    for (const CellExecution& cell : report.cells) {
        output << "Cell " << cell.cell_index + 1 << " [" << cell.execution_count << "]\n";
        output << cell.output;
        error << cell.error;
    }
    if (!report.success) {
        error << "notebook stopped at cell " << report.failed_cell + 1 << '\n';
    }
}

int run_notebook(const std::filesystem::path& path, bool update, std::ostream& output, std::ostream& error) {
    Document document = read(path);
    Kernel kernel(path.parent_path().empty() ? std::filesystem::current_path() : path.parent_path());
    const ExecutionReport report = kernel.execute(document);
    apply_execution(document, report);
    print_report(report, output, error);
    if (update) {
        write(document, path);
    }
    return report.success ? 0 : 1;
}

int check_notebook(const std::filesystem::path& path, std::ostream& output, std::ostream& error) {
    const Document expected = read(path);
    Document actual = expected;
    Kernel kernel(path.parent_path().empty() ? std::filesystem::current_path() : path.parent_path());
    const ExecutionReport report = kernel.execute(actual);
    apply_execution(actual, report);
    if (!report.success) {
        print_report(report, output, error);
        return 1;
    }

    bool drift = false;
    for (std::size_t index = 0; index < expected.cells.size(); ++index) {
        if (expected.cells[index].kind != CellKind::code) {
            continue;
        }
        if (expected.cells[index].output != actual.cells[index].output ||
            expected.cells[index].error != actual.cells[index].error) {
            error << "output drift in cell " << index + 1 << '\n';
            drift = true;
        }
    }
    if (drift) {
        error << "run 'dune notebook run " << path.string() << " --update' to refresh saved outputs\n";
        return 1;
    }
    output << "notebook outputs are current: " << path.string() << '\n';
    return 0;
}

int export_notebook(const std::vector<std::string>& arguments, std::ostream& output) {
    if (arguments.size() < 2) {
        throw std::runtime_error("notebook export requires a .dnb path");
    }
    const std::filesystem::path input = arguments[1];
    std::filesystem::path destination = input;
    destination.replace_extension(".html");
    bool html_requested = false;
    for (std::size_t index = 2; index < arguments.size(); ++index) {
        if (arguments[index] == "--html") {
            html_requested = true;
        } else if (arguments[index] == "-o" && index + 1 < arguments.size()) {
            destination = arguments[++index];
        } else {
            throw std::runtime_error("unknown notebook export option '" + arguments[index] + "'");
        }
    }
    if (!html_requested) {
        throw std::runtime_error("notebook export currently requires --html");
    }

    const Document document = read(input);
    if (!destination.parent_path().empty()) {
        std::filesystem::create_directories(destination.parent_path());
    }
    std::ofstream file(destination, std::ios::binary);
    if (!file) {
        throw std::runtime_error("could not open export path '" + destination.string() + "'");
    }
    file << render_html(document);
    output << "exported " << destination.string() << '\n';
    return 0;
}

int create_notebook(const std::vector<std::string>& arguments, std::ostream& output) {
    if (arguments.size() < 2 || arguments.size() > 4) {
        throw std::runtime_error("notebook new requires a .dnb path and optional --title");
    }
    const std::filesystem::path path = arguments[1];
    if (path.extension() != ".dnb") {
        throw std::runtime_error("notebook path must use the .dnb extension");
    }
    std::string title = path.stem().string();
    if (arguments.size() > 2) {
        if (arguments.size() != 4 || arguments[2] != "--title") {
            throw std::runtime_error("expected --title <text>");
        }
        title = arguments[3];
    }
    if (std::filesystem::exists(path)) {
        throw std::runtime_error("notebook already exists: " + path.string());
    }
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("could not create notebook '" + path.string() + "'");
    }
    file << empty_document_json(title);
    output << "created " << path.string() << '\n';
    return 0;
}

unsigned short parse_port(const std::string& value) {
    try {
        std::size_t parsed_characters = 0;
        const unsigned long parsed = std::stoul(value, &parsed_characters);
        if (parsed_characters != value.size() || parsed > std::numeric_limits<unsigned short>::max()) {
            throw std::runtime_error("port out of range");
        }
        return static_cast<unsigned short>(parsed);
    } catch (const std::exception&) {
        throw std::runtime_error("invalid notebook port '" + value + "'");
    }
}

int serve_notebooks(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& error) {
    ServerOptions options;
    bool location_set = false;
    for (std::size_t index = 1; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "--host" && index + 1 < arguments.size()) {
            options.host = arguments[++index];
        } else if (argument == "--port" && index + 1 < arguments.size()) {
            options.port = parse_port(arguments[++index]);
        } else if (argument == "--token" && index + 1 < arguments.size()) {
            options.token = arguments[++index];
        } else if (argument == "--no-open") {
            options.open_browser = false;
        } else if (!argument.starts_with('-') && !location_set) {
            const std::filesystem::path location = argument;
            location_set = true;
            if (location.extension() == ".dnb") {
                if (!std::filesystem::is_regular_file(location)) {
                    throw std::runtime_error("notebook does not exist: " + location.string());
                }
                options.initial_notebook = std::filesystem::absolute(location);
                options.root = options.initial_notebook.parent_path();
            } else {
                options.root = location;
            }
        } else {
            throw std::runtime_error("unknown notebook serve option '" + argument + "'");
        }
    }
    return serve(options, output, error);
}

} // namespace

int run_cli(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& error) {
    if (arguments.empty() || arguments[0] == "--help" || arguments[0] == "help") {
        print_usage(output);
        return 0;
    }

    if (arguments[0] == "new") {
        return create_notebook(arguments, output);
    }
    if (arguments[0] == "run") {
        if (arguments.size() < 2 || arguments.size() > 3) {
            throw std::runtime_error("notebook run requires a .dnb path and optional --update");
        }
        const bool update = arguments.size() == 3 && arguments[2] == "--update";
        if (arguments.size() == 3 && !update) {
            throw std::runtime_error("unknown notebook run option '" + arguments[2] + "'");
        }
        return run_notebook(arguments[1], update, output, error);
    }
    if (arguments[0] == "check") {
        if (arguments.size() != 2) {
            throw std::runtime_error("notebook check requires exactly one .dnb path");
        }
        return check_notebook(arguments[1], output, error);
    }
    if (arguments[0] == "export") {
        return export_notebook(arguments, output);
    }
    if (arguments[0] == "serve") {
        return serve_notebooks(arguments, output, error);
    }

    throw std::runtime_error("unknown notebook command '" + arguments[0] + "'");
}

void print_usage(std::ostream& output) {
    output << "usage:\n";
    output << "  dune notebook new <file.dnb> [--title <text>]\n";
    output << "  dune notebook run <file.dnb> [--update]\n";
    output << "  dune notebook check <file.dnb>\n";
    output << "  dune notebook export <file.dnb> --html [-o <file.html>]\n";
    output << "  dune notebook serve [file.dnb|directory] [--host <ip>] [--port <port>] [--token <token>] "
              "[--no-open]\n";
}

} // namespace dune::notebook
