#include "notebook/notebook.hpp"
#include "notebook/server.hpp"
#include "notebook/web_assets.hpp"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

std::filesystem::path temporary_directory() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("dune-notebook-tests-" + std::to_string(stamp));
    std::filesystem::create_directories(path);
    return path;
}

struct TemporaryDirectory {
    std::filesystem::path path = temporary_directory();

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

dune::notebook::Document sample_document(const std::filesystem::path& path = {}) {
    dune::notebook::Document document;
    document.title = "State & gradients";
    document.path = path;
    document.cells = {
        dune::notebook::Cell{"intro", dune::notebook::CellKind::markdown, "# Stateful notebook", {}, {}, 0},
        dune::notebook::Cell{"binding", dune::notebook::CellKind::code, "x = 40 + 2;", {}, {}, 0},
        dune::notebook::Cell{"value", dune::notebook::CellKind::code, "x", "42\n", {}, 0},
    };
    return document;
}

bool parses_and_serializes_versioned_dnb_documents() {
    const dune::notebook::Document original = sample_document();
    const std::string encoded = dune::notebook::serialize(original);
    const dune::notebook::Document decoded = dune::notebook::parse(encoded);
    const dune::notebook::Document array_source = dune::notebook::parse(R"json({
        "dune_notebook": 1,
        "metadata": {"title": "\u0041rray \ud83d\ude80", "future_field": true},
        "cells": [{
            "id": "one",
            "cell_type": "code",
            "source": ["1 + ", "2"],
            "execution_count": null,
            "outputs": [],
            "future_field": {"ignored": true}
        }]
    })json");

    bool passed = true;
    passed = expect(decoded.format_version == 1, "expected format version 1") && passed;
    passed = expect(decoded.title == original.title, "expected metadata title round-trip") && passed;
    passed = expect(decoded.cells.size() == 3, "expected all cells to round-trip") && passed;
    passed = expect(decoded.cells[2].output == "42\n", "expected structured stdout round-trip") && passed;
    passed =
        expect(array_source.cells[0].source == "1 + 2", "expected ipynb-style source arrays to be accepted") && passed;
    passed =
        expect(array_source.title == "Array 🚀", "expected escaped Unicode and surrogate pairs to decode") && passed;
    try {
        dune::notebook::parse("{\"dune_notebook\":2,\"cells\":[]}");
        passed = expect(false, "expected an unsupported version error") && passed;
    } catch (const std::exception& error) {
        passed = expect(std::string(error.what()).find("unsupported") != std::string::npos,
                        "expected a clear unsupported-version diagnostic") &&
                 passed;
    }
    try {
        dune::notebook::parse(R"json({
            "dune_notebook": 1,
            "cells": [
                {"id": "same", "cell_type": "markdown", "source": ""},
                {"id": "same", "cell_type": "code", "source": "", "outputs": []}
            ]
        })json");
        passed = expect(false, "expected duplicate cell IDs to be rejected") && passed;
    } catch (const std::exception& error) {
        passed = expect(std::string(error.what()).find("duplicate cell id") != std::string::npos,
                        "expected a duplicate-cell diagnostic") &&
                 passed;
    }
    return passed;
}

bool executes_stateful_cells_and_recovers_after_edits() {
    TemporaryDirectory temporary;
    dune::notebook::Document document = sample_document(temporary.path / "state.dnb");
    dune::notebook::Kernel kernel(temporary.path);

    dune::notebook::ExecutionReport report = kernel.execute(document);
    dune::notebook::apply_execution(document, report);

    bool passed = true;
    passed = expect(report.success, "expected stateful notebook execution to succeed") && passed;
    passed = expect(report.cells.size() == 2, "expected both code cells to execute") && passed;
    passed = expect(document.cells[2].output == "42\n", "expected the second cell to see the first binding") && passed;

    document.cells.push_back(
        dune::notebook::Cell{"bad", dune::notebook::CellKind::code, "bad: int = true;", {}, {}, 0});
    document.cells.push_back(dune::notebook::Cell{"after", dune::notebook::CellKind::code, "x + 1", {}, {}, 0});
    report = kernel.execute(document);
    passed = expect(!report.success && report.failed_cell == 3, "expected execution to stop at the bad cell") && passed;
    passed = expect(report.cells.back().error.find("state.dnb#cell-bad:1:1") != std::string::npos,
                    "expected notebook path, cell id, and local line in diagnostics") &&
             passed;

    document.cells[3].source = "y: int = 1;";
    report = kernel.execute(document);
    dune::notebook::apply_execution(document, report);
    passed = expect(report.success, "expected the edited cell to recover the kernel") && passed;
    passed =
        expect(document.cells[4].output == "43\n", "expected execution to continue after the repaired cell") && passed;

    document.cells[1].source = "x = 10;";
    report = kernel.execute(document, 2);
    dune::notebook::apply_execution(document, report);
    passed = expect(report.success && document.cells[2].output == "10\n",
                    "expected editing an earlier cell to rebuild dependent state") &&
             passed;

    kernel.reset();
    report = kernel.execute(document, 2);
    passed = expect(report.success && report.cells[0].execution_count == 1 && report.cells[1].execution_count == 2,
                    "expected Restart Kernel to reset execution counts") &&
             passed;

    document.cells.push_back(dune::notebook::Cell{"runtime", dune::notebook::CellKind::code, "1 / 0", {}, {}, 0});
    report = kernel.execute(document);
    passed = expect(!report.success && report.cells.back().error.find("state.dnb#cell-runtime") != std::string::npos,
                    "expected runtime failures to identify their notebook cell") &&
             passed;
    return passed;
}

bool renders_latex_math_in_markdown_and_static_exports() {
    dune::notebook::Document document;
    document.title = "Math notebook";
    document.cells = {dune::notebook::Cell{"math",
                                           dune::notebook::CellKind::markdown,
                                           R"markdown(# Mathematical notation

Inline $E = mc^2$, \(\alpha + \beta\), and $\bar{x}$.

$$
\sum_{i=1}^{n} x_i = \frac{-b \pm \sqrt{b^2 - 4ac}}{2a}
$$

\[
\begin{pmatrix}a & b \\ c & d\end{pmatrix}
\]

Safe $\text{<script>alert(1)</script>}$, literal `$not_math$`, and escaped \$5.)markdown",
                                           {},
                                           {},
                                           0}};

    const std::string html = dune::notebook::render_html(document);
    bool passed = true;
    passed = expect(html.find("<math class=\"latex-math\"") != std::string::npos &&
                        html.find("display=\"inline\"") != std::string::npos &&
                        html.find("display=\"block\"") != std::string::npos,
                    "expected inline and display LaTeX to render as native MathML") &&
             passed;
    passed = expect(html.find("<mfrac>") != std::string::npos && html.find("<msqrt>") != std::string::npos &&
                        html.find("<munderover>") != std::string::npos && html.find("<mtable") != std::string::npos,
                    "expected fractions, roots, limits, and matrices in exported formulas") &&
             passed;
    passed = expect(html.find("<mrow class=\"latex-overbar\">") != std::string::npos &&
                        html.find(".latex-overbar{border-top:.055em solid currentColor;padding-top:.16em}") !=
                            std::string::npos &&
                        html.find("‾") == std::string::npos,
                    "expected a separated CSS rule instead of a merging overbar glyph") &&
             passed;
    const std::string_view notebook_app = dune::notebook::notebook_app_html();
    passed = expect(notebook_app.find(".latex-overbar {") != std::string_view::npos &&
                        notebook_app.find("border-top: .055em solid currentColor;") != std::string_view::npos &&
                        notebook_app.find("padding-top: .16em;") != std::string_view::npos &&
                        notebook_app.find("<mrow class=\"latex-overbar\">") != std::string_view::npos &&
                        notebook_app.find("‾") == std::string_view::npos,
                    "expected the interactive renderer to use the same separated overbar rule") &&
             passed;
    passed = expect(html.find("annotation encoding=\"application/x-tex\"") != std::string::npos &&
                        html.find("<code>$not_math$</code>") != std::string::npos &&
                        html.find("application/x-tex\">not_math") == std::string::npos,
                    "expected accessible TeX annotations without parsing code spans") &&
             passed;
    passed = expect(html.find("<script>alert(1)</script>") == std::string::npos &&
                        html.find("&lt;script&gt;alert(1)&lt;/script&gt;") != std::string::npos,
                    "expected formula text to remain HTML escaped") &&
             passed;
    return passed;
}

bool serves_secure_workspace_routes() {
    TemporaryDirectory temporary;
    const std::filesystem::path path = temporary.path / "demo.dnb";
    dune::notebook::write(sample_document(path), path);
    dune::notebook::Service service(temporary.path, "secret");
    const std::map<std::string, std::string> authorization{{"x-dune-token", "secret"}};

    bool passed = true;
    passed = expect(service.handle({"GET", "/api/health", {}, {}}).status == 200,
                    "expected an unauthenticated health route") &&
             passed;
    passed = expect(service.handle({"GET", "/api/files", {}, {}}).status == 401,
                    "expected workspace routes to require a token") &&
             passed;

    const dune::notebook::HttpResponse files = service.handle({"GET", "/api/files", authorization, {}});
    passed = expect(files.status == 200 && files.body.find("demo.dnb") != std::string::npos,
                    "expected the file browser to list .dnb documents") &&
             passed;
    passed = expect(service.handle({"GET", "/api/notebook?path=../escape.dnb", authorization, {}}).status == 400,
                    "expected path traversal to be rejected") &&
             passed;

    const dune::notebook::HttpResponse loaded =
        service.handle({"GET", "/api/notebook?path=demo.dnb", authorization, {}});
    passed = expect(loaded.status == 200 && loaded.body.find("\"dune_notebook\": 1") != std::string::npos,
                    "expected notebook loading through the API") &&
             passed;
    const std::map<std::string, std::string> create_only{{"x-dune-token", "secret"}, {"if-none-match", "*"}};
    passed = expect(service.handle({"PUT", "/api/notebook?path=demo.dnb", create_only,
                                    dune::notebook::serialize(sample_document(path))})
                            .status == 409,
                    "expected notebook creation to preserve an existing document") &&
             passed;

    const dune::notebook::HttpResponse created = service.handle({"POST", "/api/sessions", authorization, "demo.dnb"});
    passed = expect(created.status == 201 && created.body.find("session-1") != std::string::npos,
                    "expected a persistent kernel session") &&
             passed;

    const dune::notebook::HttpResponse executed =
        service.handle({"POST", "/api/sessions/session-1/execute?cell=all", authorization,
                        dune::notebook::serialize(sample_document(path))});
    passed = expect(executed.status == 200 && executed.body.find("\"success\":true") != std::string::npos,
                    "expected session execution to return an updated JSON document") &&
             passed;
    passed = expect(executed.body.find("\"text\": \"42\\n\"") != std::string::npos,
                    "expected structured stdout in the API response") &&
             passed;

    const dune::notebook::HttpResponse exported =
        service.handle({"POST", "/api/export", authorization, dune::notebook::serialize(sample_document(path))});
    passed = expect(exported.status == 200 && exported.content_type.starts_with("text/html") &&
                        exported.body.find("&amp;") != std::string::npos,
                    "expected standalone escaped HTML export") &&
             passed;
    passed = expect(exported.body.find("prefers-color-scheme:dark") != std::string::npos &&
                        exported.body.find("Static export") != std::string::npos &&
                        exported.body.find("class=\"prompt\">In [") != std::string::npos,
                    "expected Jupyter-style light/dark HTML export layout") &&
             passed;
    dune::notebook::Document chart_document = sample_document(path);
    chart_document.cells[2].output =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"40\" height=\"20\"><script>alert(1)</script>"
        "<rect width=\"40\" height=\"20\"/></svg>\n";
    const std::string chart_export = dune::notebook::render_html(chart_document);
    passed = expect(chart_export.find("class=\"rich-output\"") != std::string::npos &&
                        chart_export.find("data:image/svg+xml;charset=utf-8,") != std::string::npos &&
                        chart_export.find("<script>") == std::string::npos,
                    "expected SVG output to use a non-executable image data URI") &&
             passed;
    passed = expect(service.handle({"DELETE", "/api/sessions/session-1", authorization, {}}).status == 204,
                    "expected session deletion") &&
             passed;
    return passed;
}

bool rejects_unsafe_server_tokens() {
    TemporaryDirectory temporary;
    dune::notebook::ServerOptions options;
    options.root = temporary.path;
    options.token = "unsafe;token";
    options.open_browser = false;
    std::ostringstream output;
    std::ostringstream error;

    try {
        dune::notebook::serve(options, output, error);
        return expect(false, "expected shell-unsafe server tokens to be rejected");
    } catch (const std::exception& failure) {
        return expect(std::string(failure.what()).find("only letters") != std::string::npos,
                      "expected a clear invalid-token diagnostic");
    }
}

#if defined(_WIN32)
using TestSocket = SOCKET;
constexpr TestSocket invalid_test_socket = INVALID_SOCKET;
#else
using TestSocket = int;
constexpr TestSocket invalid_test_socket = -1;
#endif

void close_test_socket(TestSocket socket) {
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
}

std::string http_get(unsigned short port, const std::string& target) {
#if defined(_WIN32)
    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        throw std::runtime_error("could not initialize test Winsock");
    }
#endif
    TestSocket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == invalid_test_socket) {
        throw std::runtime_error("could not create test socket");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_test_socket(socket);
        throw std::runtime_error("could not connect to notebook test server");
    }
    const std::string request = "GET " + target + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    send(socket, request.data(), static_cast<int>(request.size()), 0);
    std::string response;
    char buffer[4096];
    while (true) {
#if defined(_WIN32)
        const int count = recv(socket, buffer, sizeof(buffer), 0);
#else
        // The analyzer carries libc++'s future mutex into this independent
        // client helper even though future::get() has already returned.
        // NOLINTNEXTLINE(clang-analyzer-unix.BlockInCriticalSection)
        const ssize_t count = recv(socket, buffer, sizeof(buffer), 0);
#endif
        if (count <= 0) {
            break;
        }
        response.append(buffer, static_cast<std::size_t>(count));
    }
    close_test_socket(socket);
#if defined(_WIN32)
    WSACleanup();
#endif
    return response;
}

bool accepts_real_http_connections() {
    TemporaryDirectory temporary;
    std::promise<unsigned short> ready;
    std::future<unsigned short> port = ready.get_future();
    std::exception_ptr server_failure;
    std::ostringstream server_output;
    std::ostringstream server_error;
    dune::notebook::ServerOptions options;
    options.root = temporary.path;
    options.port = 0;
    options.open_browser = false;
    options.token = "test-token";
    options.max_requests = 2;
    options.on_ready = [&ready](unsigned short actual_port, const std::string&) { ready.set_value(actual_port); };

    std::thread server([&] {
        try {
            dune::notebook::serve(options, server_output, server_error);
        } catch (...) {
            server_failure = std::current_exception();
            try {
                ready.set_exception(server_failure);
            } catch (const std::future_error& already_satisfied) {
                static_cast<void>(already_satisfied);
            }
        }
    });

    bool passed = true;
    try {
        const unsigned short actual_port = port.get();
        const std::string health = http_get(actual_port, "/api/health");
        const std::string app = http_get(actual_port, "/");
        passed = expect(health.find("HTTP/1.1 200 OK") != std::string::npos &&
                            health.find("{\"status\":\"ok\"}") != std::string::npos,
                        "expected a real HTTP health response") &&
                 passed;
        passed = expect(app.find("Dune Notebook") != std::string::npos &&
                            app.find("Content-Security-Policy:") != std::string::npos &&
                            app.find("Notebook toolbar") != std::string::npos &&
                            app.find("dune-notebook-theme") != std::string::npos &&
                            app.find("Selected cell type") != std::string::npos &&
                            app.find("cell-ring-travel") != std::string::npos &&
                            app.find("animation: cell-ring-travel 6s linear infinite") != std::string::npos &&
                            app.find("function lexDune(source)") != std::string::npos &&
                            app.find("syntax-keyword") != std::string::npos &&
                            app.find("syncCodeHighlight(source, highlight)") != std::string::npos &&
                            app.find("Clear selected output") != std::string::npos &&
                            app.find("Clear all outputs") != std::string::npos &&
                            app.find("Restart kernel and clear all outputs") != std::string::npos &&
                            app.find("function clearSelectedOutput()") != std::string::npos &&
                            app.find("function clearAllOutputs(") != std::string::npos &&
                            app.find("class LatexMathParser") != std::string::npos &&
                            app.find("function renderLatexMath(") != std::string::npos &&
                            app.find("application/x-tex") != std::string::npos &&
                            app.find("math-display") != std::string::npos &&
                            app.find("data:image/svg+xml") != std::string::npos &&
                            app.find("img-src 'self' data:") != std::string::npos &&
                            app.find("creates one at the end") != std::string::npos,
                        "expected the themed Jupyter-style browser application and security headers") &&
                 passed;
    } catch (const std::exception& error) {
        passed = expect(false, std::string("HTTP integration failed: ") + error.what()) && passed;
    }
    server.join();
    if (server_failure) {
        try {
            std::rethrow_exception(server_failure);
        } catch (const std::exception& error) {
            passed = expect(false, std::string("server failed: ") + error.what()) && passed;
        }
    }
    return passed;
}

} // namespace

int main() {
    bool passed = true;
    passed = parses_and_serializes_versioned_dnb_documents() && passed;
    passed = executes_stateful_cells_and_recovers_after_edits() && passed;
    passed = renders_latex_math_in_markdown_and_static_exports() && passed;
    passed = serves_secure_workspace_routes() && passed;
    passed = rejects_unsafe_server_tokens() && passed;
    passed = accepts_real_http_connections() && passed;
    return passed ? 0 : 1;
}
