#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace dune::cli {

// Docker-style build progress reporter for `dune build`.
//
// On an interactive terminal each phase renders an animated braille spinner
// that collapses in place to a green check mark on success or a red cross on
// failure, followed by a dim elapsed-time suffix. When stderr is not a TTY
// (pipes, CI logs) the reporter falls back to one plain, spinner-free line per
// phase so redirected output stays stable and easy to diff. ANSI colors follow
// the usual DUNE_COLOR / NO_COLOR conventions.
//
// The interface intentionally mirrors the CliReporter used by the other
// subcommands (begin / done / error) so the same run_step helper drives both.
class BuildProgress {
public:
    explicit BuildProgress(const std::string& command);
    ~BuildProgress();

    BuildProgress(const BuildProgress&) = delete;
    BuildProgress& operator=(const BuildProgress&) = delete;
    BuildProgress(BuildProgress&&) = delete;
    BuildProgress& operator=(BuildProgress&&) = delete;

    // Marks the start of a phase. On a TTY this starts the spinner animation;
    // otherwise it only records the start time used for the elapsed suffix.
    void begin(std::string_view step);

    // Records the source text + path so a failed phase can render a snippet.
    void set_source(std::string source, std::string filename);

    // Resolves the in-flight phase as succeeded or failed.
    void done(std::string_view step);
    void error(std::string_view step, const std::exception& error);

private:
    void animate();
    void paint_spinner_locked();
    [[nodiscard]] double elapsed_seconds_locked() const;
    [[nodiscard]] std::string colorize(std::string_view text, std::string_view ansi) const;

    bool tty_ = false;
    bool color_ = false;

    std::string source_;
    std::string filename_;
    bool has_source_ = false;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stop_ = false;
    bool active_ = false;
    std::size_t frame_ = 0;
    std::string label_;
    std::chrono::steady_clock::time_point start_{};
};

} // namespace dune::cli
