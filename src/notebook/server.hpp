#pragma once

#include "notebook/notebook.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>

namespace dune::notebook {

struct HttpRequest {
    std::string method;
    std::string target;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string content_type = "text/plain; charset=utf-8";
    std::map<std::string, std::string> headers;
    std::string body;
};

struct ServerOptions {
    std::filesystem::path root = std::filesystem::current_path();
    std::filesystem::path initial_notebook;
    std::string host = "127.0.0.1";
    unsigned short port = 8888;
    bool open_browser = true;
    std::string token;
    std::size_t max_requests = 0;
    std::function<void(unsigned short, const std::string&)> on_ready;
};

class Service {
public:
    Service(const std::filesystem::path& root, std::string token);

    HttpResponse handle(const HttpRequest& request);
    const std::filesystem::path& root() const;
    const std::string& token() const;

private:
    struct Session {
        std::filesystem::path path;
        std::unique_ptr<Kernel> kernel;
    };

    std::filesystem::path resolve_path(const std::string& requested, bool must_exist) const;
    HttpResponse error_response(int status, const std::string& message) const;
    bool authorized(const HttpRequest& request) const;

    std::filesystem::path root_;
    std::string token_;
    std::unordered_map<std::string, Session> sessions_;
    std::size_t next_session_id_ = 1;
};

std::string empty_document_json(const std::string& title);
int serve(const ServerOptions& options, std::ostream& output, std::ostream& error);

} // namespace dune::notebook
