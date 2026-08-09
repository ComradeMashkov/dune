#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace dune::repl {

struct Options {
    std::string version;
    std::filesystem::path source_directory;
    bool show_prompts = false;
};

struct EvaluationResult {
    bool success = false;
    std::string output;
    std::string error;
};

class Session {
public:
    explicit Session(std::filesystem::path source_directory);

    EvaluationResult evaluate(const std::string& source, const std::string& source_name = "<repl>");
    void reset();

private:
    struct SourceEntry {
        std::size_t first_line = 1;
        std::size_t last_line = 1;
        std::string source_name;
    };

    std::filesystem::path source_directory_;
    std::string source_;
    std::vector<SourceEntry> source_entries_;
    std::string previous_output_;
    std::string previous_error_;
};

int run(std::istream& input, std::ostream& output, std::ostream& error, const Options& options);

} // namespace dune::repl
