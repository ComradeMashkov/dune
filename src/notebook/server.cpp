#include "server.hpp"

#include "notebook/web_assets.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace dune::notebook {

namespace {

#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
#endif

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

std::string_view trim_ascii(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

int hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::string percent_decode(std::string_view value) {
    std::string decoded;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2 < value.size()) {
            const int high = hex_value(value[index + 1]);
            const int low = hex_value(value[index + 2]);
            if (high >= 0 && low >= 0) {
                decoded += static_cast<char>((high << 4) | low);
                index += 2;
                continue;
            }
        }
        decoded += value[index] == '+' ? ' ' : value[index];
    }
    return decoded;
}

std::string percent_encode(std::string_view value) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char character : value) {
        if (std::isalnum(character) != 0 || character == '-' || character == '_' || character == '.' ||
            character == '~' || character == '/') {
            encoded += static_cast<char>(character);
        } else {
            encoded += '%';
            encoded += digits[character >> 4];
            encoded += digits[character & 0x0f];
        }
    }
    return encoded;
}

std::string json_escape(std::string_view text) {
    std::string escaped;
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char character : text) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (character < 0x20) {
                escaped += "\\u00";
                escaped += digits[character >> 4];
                escaped += digits[character & 0x0f];
            } else {
                escaped += static_cast<char>(character);
            }
            break;
        }
    }
    return escaped;
}

std::pair<std::string, std::map<std::string, std::string>> split_target(const std::string& target) {
    const std::size_t question = target.find('?');
    const std::string path = question == std::string::npos ? target : target.substr(0, question);
    std::map<std::string, std::string> query;
    if (question == std::string::npos) {
        return {path, query};
    }

    std::size_t position = question + 1;
    while (position <= target.size()) {
        const std::size_t ampersand = target.find('&', position);
        const std::string_view part = std::string_view(target).substr(
            position, ampersand == std::string::npos ? target.size() - position : ampersand - position);
        const std::size_t equal = part.find('=');
        const std::string key = percent_decode(part.substr(0, equal));
        const std::string value = equal == std::string_view::npos ? "" : percent_decode(part.substr(equal + 1));
        query.insert_or_assign(key, value);
        if (ampersand == std::string::npos) {
            break;
        }
        position = ampersand + 1;
    }
    return {path, query};
}

bool is_within(const std::filesystem::path& root, const std::filesystem::path& candidate) {
    auto root_part = root.begin();
    auto candidate_part = candidate.begin();
    for (; root_part != root.end() && candidate_part != candidate.end(); ++root_part, ++candidate_part) {
        if (*root_part != *candidate_part) {
            return false;
        }
    }
    return root_part == root.end();
}

std::string random_token() {
    std::random_device random;
    std::ostringstream token;
    token << std::hex << std::setfill('0');
    for (int index = 0; index < 4; ++index) {
        token << std::setw(8) << random();
    }
    return token.str();
}

bool is_safe_token(std::string_view token) {
    return !token.empty() && token.size() <= 128 &&
           std::all_of(token.begin(), token.end(), [](unsigned char character) {
               return std::isalnum(character) != 0 || character == '-' || character == '_';
           });
}

std::size_t parse_decimal_size(std::string_view value, std::string_view field) {
    if (value.empty()) {
        throw std::runtime_error("invalid " + std::string(field));
    }
    std::size_t parsed = 0;
    for (const unsigned char character : value) {
        if (std::isdigit(character) == 0) {
            throw std::runtime_error("invalid " + std::string(field));
        }
        const std::size_t digit = character - '0';
        if (parsed > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
            throw std::runtime_error(std::string(field) + " is too large");
        }
        parsed = parsed * 10 + digit;
    }
    return parsed;
}

std::string status_text(int status) {
    switch (status) {
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 204:
        return "No Content";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 409:
        return "Conflict";
    case 413:
        return "Payload Too Large";
    case 500:
        return "Internal Server Error";
    default:
        return "Error";
    }
}

void close_socket(Socket socket) {
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
}

void send_all(Socket socket, std::string_view bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
#if defined(_WIN32)
        const int count = send(socket, bytes.data() + sent, static_cast<int>(bytes.size() - sent), 0);
#elif defined(MSG_NOSIGNAL)
        const ssize_t count = send(socket, bytes.data() + sent, bytes.size() - sent, MSG_NOSIGNAL);
#else
        const ssize_t count = send(socket, bytes.data() + sent, bytes.size() - sent, 0);
#endif
        if (count <= 0) {
            throw std::runtime_error("failed to send HTTP response");
        }
        sent += static_cast<std::size_t>(count);
    }
}

HttpRequest receive_request(Socket socket) {
    constexpr std::size_t max_header = std::size_t{64} * 1024;
    constexpr std::size_t max_body = std::size_t{16} * 1024 * 1024;
    std::string bytes;
    std::array<char, 8192> buffer{};
    std::size_t header_end = std::string::npos;
    while ((header_end = bytes.find("\r\n\r\n")) == std::string::npos) {
#if defined(_WIN32)
        const int count = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
        const ssize_t count = recv(socket, buffer.data(), buffer.size(), 0);
#endif
        if (count <= 0) {
            throw std::runtime_error("connection closed before HTTP headers");
        }
        bytes.append(buffer.data(), static_cast<std::size_t>(count));
        if (bytes.size() > max_header) {
            throw std::runtime_error("HTTP headers exceed 64 KiB");
        }
    }

    const std::string headers_text = bytes.substr(0, header_end);
    std::istringstream headers(headers_text);
    HttpRequest request;
    std::string version;
    if (!(headers >> request.method >> request.target >> version) || !version.starts_with("HTTP/1.")) {
        throw std::runtime_error("invalid HTTP request line");
    }
    std::string line;
    std::getline(headers, line);
    while (std::getline(headers, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos) {
            throw std::runtime_error("invalid HTTP header");
        }
        request.headers.insert_or_assign(lower_ascii(std::string(trim_ascii(std::string_view(line).substr(0, colon)))),
                                         std::string(trim_ascii(std::string_view(line).substr(colon + 1))));
    }

    std::size_t content_length = 0;
    if (const auto length = request.headers.find("content-length"); length != request.headers.end()) {
        content_length = parse_decimal_size(length->second, "Content-Length");
    }
    if (content_length > max_body) {
        throw std::runtime_error("HTTP body exceeds 16 MiB");
    }

    const std::size_t body_start = header_end + 4;
    request.body = bytes.substr(body_start);
    while (request.body.size() < content_length) {
#if defined(_WIN32)
        const int count = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
        const ssize_t count = recv(socket, buffer.data(), buffer.size(), 0);
#endif
        if (count <= 0) {
            throw std::runtime_error("connection closed before HTTP body");
        }
        request.body.append(buffer.data(), static_cast<std::size_t>(count));
        if (request.body.size() > max_body) {
            throw std::runtime_error("HTTP body exceeds 16 MiB");
        }
    }
    request.body.resize(content_length);
    return request;
}

std::string encode_response(const HttpResponse& response) {
    std::ostringstream encoded;
    encoded << "HTTP/1.1 " << response.status << ' ' << status_text(response.status) << "\r\n";
    encoded << "Content-Type: " << response.content_type << "\r\n";
    encoded << "Content-Length: " << response.body.size() << "\r\n";
    encoded << "Connection: close\r\n";
    encoded << "Cache-Control: no-store\r\n";
    encoded << "X-Content-Type-Options: nosniff\r\n";
    encoded << "X-Frame-Options: DENY\r\n";
    encoded << "Referrer-Policy: no-referrer\r\n";
    encoded << "Content-Security-Policy: default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
               "connect-src 'self'; object-src 'none'; frame-ancestors 'none'\r\n";
    for (const auto& [name, value] : response.headers) {
        encoded << name << ": " << value << "\r\n";
    }
    encoded << "\r\n" << response.body;
    return encoded.str();
}

void open_browser(const std::string& url) {
#if defined(_WIN32)
    const std::string command = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
    const std::string command = "open \"" + url + "\" >/dev/null 2>&1 &";
#else
    const std::string command = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
#endif
    std::system(command.c_str());
}

} // namespace

Service::Service(const std::filesystem::path& root, std::string token) : token_(std::move(token)) {
    std::error_code error;
    root_ = std::filesystem::weakly_canonical(std::filesystem::absolute(root), error);
    if (error || !std::filesystem::is_directory(root_)) {
        throw std::runtime_error("notebook root must be an existing directory");
    }
}

HttpResponse Service::handle(const HttpRequest& request) {
    try {
        const auto [path, query] = split_target(request.target);
        if (path == "/" && request.method == "GET") {
            return HttpResponse{200, "text/html; charset=utf-8", {}, std::string(notebook_app_html())};
        }
        if (path == "/api/health" && request.method == "GET") {
            return HttpResponse{200, "application/json; charset=utf-8", {}, "{\"status\":\"ok\"}\n"};
        }
        if (!path.starts_with("/api/")) {
            return error_response(404, "route not found");
        }
        if (!authorized(request)) {
            return error_response(401, "invalid notebook token");
        }

        if (path == "/api/files" && request.method == "GET") {
            std::vector<std::string> files;
            std::error_code error;
            for (std::filesystem::recursive_directory_iterator iterator(
                     root_, std::filesystem::directory_options::skip_permission_denied, error);
                 !error && iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error)) {
                if (!iterator->is_regular_file(error) || iterator->path().extension() != ".dnb") {
                    continue;
                }
                const std::filesystem::path canonical = std::filesystem::weakly_canonical(iterator->path(), error);
                if (!error && is_within(root_, canonical)) {
                    files.push_back(std::filesystem::relative(canonical, root_).generic_string());
                }
            }
            std::sort(files.begin(), files.end());
            std::ostringstream json;
            json << "{\"files\":[";
            for (std::size_t index = 0; index < files.size(); ++index) {
                json << (index == 0 ? "" : ",") << '"' << json_escape(files[index]) << '"';
            }
            json << "]}\n";
            return HttpResponse{200, "application/json; charset=utf-8", {}, json.str()};
        }

        if (path == "/api/notebook") {
            const auto requested = query.find("path");
            if (requested == query.end()) {
                return error_response(400, "missing notebook path");
            }
            if (request.method == "GET") {
                const std::filesystem::path notebook_path = resolve_path(requested->second, true);
                std::ifstream input(notebook_path, std::ios::binary);
                std::ostringstream contents;
                contents << input.rdbuf();
                const Document document = parse(contents.str(), notebook_path);
                return HttpResponse{200, "application/json; charset=utf-8", {}, serialize(document)};
            }
            if (request.method == "PUT") {
                const std::filesystem::path notebook_path = resolve_path(requested->second, false);
                const auto create_only = request.headers.find("if-none-match");
                if (create_only != request.headers.end() && create_only->second == "*" &&
                    std::filesystem::exists(notebook_path)) {
                    return error_response(409, "notebook already exists: " + requested->second);
                }
                Document document = parse(request.body, notebook_path);
                std::filesystem::create_directories(notebook_path.parent_path());
                write(document, notebook_path);
                return HttpResponse{201, "application/json; charset=utf-8", {}, "{\"saved\":true}\n"};
            }
            return error_response(405, "method not allowed");
        }

        if (path == "/api/export" && request.method == "POST") {
            const Document document = parse(request.body);
            return HttpResponse{200,
                                "text/html; charset=utf-8",
                                {{"Content-Disposition", "inline; filename=\"notebook.html\""}},
                                render_html(document)};
        }

        if (path == "/api/sessions" && request.method == "POST") {
            const std::filesystem::path notebook_path = resolve_path(request.body, true);
            const std::string id = "session-" + std::to_string(next_session_id_++);
            sessions_.insert_or_assign(id,
                                       Session{notebook_path, std::make_unique<Kernel>(notebook_path.parent_path())});
            return HttpResponse{201, "application/json; charset=utf-8", {}, "{\"id\":\"" + json_escape(id) + "\"}\n"};
        }

        constexpr std::string_view session_prefix = "/api/sessions/";
        if (path.starts_with(session_prefix)) {
            const std::string_view remainder = std::string_view(path).substr(session_prefix.size());
            const std::size_t slash = remainder.find('/');
            const std::string id(remainder.substr(0, slash));
            const auto session = sessions_.find(id);
            if (session == sessions_.end()) {
                return error_response(404, "notebook session not found");
            }
            const std::string action = slash == std::string_view::npos ? "" : std::string(remainder.substr(slash + 1));
            if (action.empty() && request.method == "DELETE") {
                sessions_.erase(session);
                return HttpResponse{204, "text/plain; charset=utf-8", {}, {}};
            }
            if (action == "reset" && request.method == "POST") {
                session->second.kernel->reset();
                return HttpResponse{200, "application/json; charset=utf-8", {}, "{\"reset\":true}\n"};
            }
            if (action == "execute" && request.method == "POST") {
                Document document = parse(request.body, session->second.path);
                std::optional<std::size_t> through_cell;
                if (const auto cell = query.find("cell"); cell != query.end() && cell->second != "all") {
                    try {
                        through_cell = parse_decimal_size(cell->second, "cell index");
                    } catch (const std::exception&) {
                        return error_response(400, "cell must be an index or 'all'");
                    }
                    if (*through_cell >= document.cells.size() ||
                        document.cells[*through_cell].kind != CellKind::code) {
                        return error_response(400, "cell index does not identify a code cell");
                    }
                }
                const ExecutionReport report = session->second.kernel->execute(document, through_cell);
                apply_execution(document, report);
                std::ostringstream json;
                json << "{\"success\":" << (report.success ? "true" : "false") << ",\"failed_cell\":";
                if (report.success) {
                    json << "null";
                } else {
                    json << report.failed_cell;
                }
                json << ",\"document\":" << serialize(document) << "}\n";
                return HttpResponse{200, "application/json; charset=utf-8", {}, json.str()};
            }
            return error_response(405, "method not allowed");
        }

        return error_response(404, "route not found");
    } catch (const std::exception& failure) {
        return error_response(400, failure.what());
    }
}

const std::filesystem::path& Service::root() const {
    return root_;
}

const std::string& Service::token() const {
    return token_;
}

std::filesystem::path Service::resolve_path(const std::string& requested, bool must_exist) const {
    if (requested.empty()) {
        throw std::runtime_error("notebook path cannot be empty");
    }
    const std::filesystem::path relative(requested);
    if (relative.is_absolute() || relative.extension() != ".dnb") {
        throw std::runtime_error("notebook path must be a relative .dnb file");
    }

    std::error_code error;
    const std::filesystem::path candidate =
        std::filesystem::weakly_canonical(root_ / relative.lexically_normal(), error);
    if (error || !is_within(root_, candidate)) {
        throw std::runtime_error("notebook path escapes the server root");
    }
    if (must_exist && !std::filesystem::is_regular_file(candidate)) {
        throw std::runtime_error("notebook does not exist: " + requested);
    }
    return candidate;
}

HttpResponse Service::error_response(int status, const std::string& message) const {
    return HttpResponse{
        status, "application/json; charset=utf-8", {}, "{\"error\":\"" + json_escape(message) + "\"}\n"};
}

bool Service::authorized(const HttpRequest& request) const {
    const auto lower = request.headers.find("x-dune-token");
    if (lower != request.headers.end()) {
        return lower->second == token_;
    }
    const auto exact = request.headers.find("X-Dune-Token");
    return exact != request.headers.end() && exact->second == token_;
}

std::string empty_document_json(const std::string& title) {
    Document document;
    document.title = title;
    document.cells = {
        Cell{"intro", CellKind::markdown, "# " + title + "\n\nWrite your notes here.", {}, {}, 0},
        Cell{"code-1", CellKind::code, {}, {}, {}, 0},
    };
    return serialize(document);
}

int serve(const ServerOptions& options, std::ostream& output, std::ostream& error) {
#if defined(_WIN32)
    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        throw std::runtime_error("could not initialize Winsock");
    }
    struct WinsockCleanup {
        ~WinsockCleanup() {
            WSACleanup();
        }
    } cleanup;
#endif

    const std::string token = options.token.empty() ? random_token() : options.token;
    if (!is_safe_token(token)) {
        throw std::runtime_error("notebook token must contain only letters, digits, '-' or '_' and be at most "
                                 "128 characters");
    }
    Service service(options.root, token);
    Socket listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == invalid_socket) {
        throw std::runtime_error("could not create notebook server socket");
    }
    struct SocketCleanup {
        Socket socket;
        ~SocketCleanup() {
            if (socket != invalid_socket) {
                close_socket(socket);
            }
        }
    } listener_cleanup{listener};

    int reuse = 1;
#if defined(_WIN32)
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(options.port);
    if (inet_pton(AF_INET, options.host.c_str(), &address.sin_addr) != 1) {
        throw std::runtime_error("notebook host must be a numeric IPv4 address");
    }
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        throw std::runtime_error("could not bind notebook server to " + options.host + ":" +
                                 std::to_string(options.port));
    }
    if (listen(listener, 16) != 0) {
        throw std::runtime_error("could not listen for notebook connections");
    }

    sockaddr_in bound{};
#if defined(_WIN32)
    int bound_size = sizeof(bound);
#else
    socklen_t bound_size = sizeof(bound);
#endif
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&bound), &bound_size) != 0) {
        throw std::runtime_error("could not read notebook server address");
    }
    const unsigned short port = ntohs(bound.sin_port);
    const std::string browser_host = options.host == "0.0.0.0" ? "127.0.0.1" : options.host;
    std::string url = "http://" + browser_host + ":" + std::to_string(port) + "/?token=" + token;
    if (!options.initial_notebook.empty()) {
        std::error_code relative_error;
        const std::filesystem::path absolute = std::filesystem::absolute(options.initial_notebook, relative_error);
        const std::filesystem::path relative = std::filesystem::relative(
            relative_error ? options.initial_notebook : absolute, service.root(), relative_error);
        if (!relative_error) {
            url += "&path=" + percent_encode(relative.generic_string());
        }
    }

    output << "Dune Notebook server\n";
    output << "  root: " << service.root().string() << '\n';
    output << "  url:  " << url << '\n';
    output << "Press Ctrl+C to stop.\n";
    output.flush();
    if (options.on_ready) {
        options.on_ready(port, url);
    }
    if (options.host == "0.0.0.0") {
        error << "warning: notebook server is listening on all IPv4 interfaces; keep the token private\n";
    }
    if (options.open_browser) {
        open_browser(url);
    }

    std::size_t handled = 0;
    while (options.max_requests == 0 || handled < options.max_requests) {
        sockaddr_in peer{};
#if defined(_WIN32)
        int peer_size = sizeof(peer);
#else
        socklen_t peer_size = sizeof(peer);
#endif
        Socket client = accept(listener, reinterpret_cast<sockaddr*>(&peer), &peer_size);
        if (client == invalid_socket) {
            throw std::runtime_error("notebook server failed to accept a connection");
        }
#if defined(SO_NOSIGPIPE)
        int no_sigpipe = 1;
        setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
        try {
            const HttpRequest request = receive_request(client);
            send_all(client, encode_response(service.handle(request)));
        } catch (const std::exception& failure) {
            send_all(client, encode_response(HttpResponse{400,
                                                          "application/json; charset=utf-8",
                                                          {},
                                                          "{\"error\":\"" + json_escape(failure.what()) + "\"}\n"}));
        }
        close_socket(client);
        ++handled;
    }
    return 0;
}

} // namespace dune::notebook
