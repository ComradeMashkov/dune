#include "cli/build_progress.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace dune::cli {

namespace {

// Classic 10-frame braille spinner (the "dots" set, U+2807..U+280F range).
constexpr std::array<std::string_view, 10> kSpinnerFrames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

constexpr std::string_view kCheckMark = "✔"; // heavy check mark
constexpr std::string_view kCrossMark = "✖"; // heavy multiplication x

constexpr std::string_view kBold = "\033[1m";
constexpr std::string_view kDim = "\033[2m";
constexpr std::string_view kGreen = "\033[32m";
constexpr std::string_view kRed = "\033[31m";
constexpr std::string_view kCyan = "\033[36m";
constexpr std::string_view kReset = "\033[0m";
constexpr std::string_view kHideCursor = "\033[?25l";
constexpr std::string_view kShowCursor = "\033[?25h";
constexpr std::string_view kClearToEol = "\033[K";

constexpr auto kFrameInterval = std::chrono::milliseconds(80);

bool stderr_is_terminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

// Mirrors the color policy used by the other CLI subcommands so `dune build`
// stays consistent: DUNE_COLOR forces the decision, NO_COLOR disables color,
// and otherwise we only colorize when stderr is a real terminal.
bool use_color() {
    const char* color = std::getenv("DUNE_COLOR");
    if (color != nullptr) {
        const std::string value = color;
        if (value == "always") {
            return true;
        }

        if (value == "never") {
            return false;
        }
    }

    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }

    return stderr_is_terminal();
}

std::string format_elapsed(double seconds) {
    std::ostringstream out;
    out << '(' << std::fixed << std::setprecision(1) << seconds << "s)";
    return out.str();
}

} // namespace

BuildProgress::BuildProgress(const std::string& command) : tty_(stderr_is_terminal()), color_(use_color()) {
    std::cerr << colorize("dune " + command, kBold) << '\n';

    if (tty_) {
        std::cerr << kHideCursor;
        std::cerr.flush();
        worker_ = std::thread([this] { animate(); });
    }
}

BuildProgress::~BuildProgress() {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
        active_ = false;
    }
    cv_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }

    if (tty_) {
        // Always restore the cursor, even when the build failed mid-phase.
        std::cerr << kShowCursor;
        std::cerr.flush();
    }
}

void BuildProgress::begin(std::string_view step) {
    if (!tty_) {
        start_ = std::chrono::steady_clock::now();
        return;
    }

    {
        const std::lock_guard<std::mutex> lock(mutex_);
        label_ = std::string(step);
        start_ = std::chrono::steady_clock::now();
        frame_ = 0;
        active_ = true;
    }
    cv_.notify_all();
}

void BuildProgress::done(std::string_view step) {
    if (!tty_) {
        std::cerr << "  " << colorize("[done]", kGreen) << ' ' << step << ' '
                  << colorize(format_elapsed(elapsed_seconds_locked()), kDim) << '\n';
        return;
    }

    const std::lock_guard<std::mutex> lock(mutex_);
    active_ = false;
    std::cerr << '\r' << kClearToEol << colorize(kCheckMark, kGreen) << ' ' << step << ' '
              << colorize(format_elapsed(elapsed_seconds_locked()), kDim) << '\n';
    std::cerr.flush();
    cv_.notify_all();
}

void BuildProgress::error(std::string_view step, std::string_view message) {
    if (!tty_) {
        std::cerr << "  " << colorize("[error]", kRed) << ' ' << step << '\n';
        std::cerr << "          " << message << '\n';
        return;
    }

    const std::lock_guard<std::mutex> lock(mutex_);
    active_ = false;
    std::cerr << '\r' << kClearToEol << colorize(kCrossMark, kRed) << ' ' << step << ' '
              << colorize(format_elapsed(elapsed_seconds_locked()), kDim) << '\n';
    std::cerr << "          " << message << '\n';
    std::cerr.flush();
    cv_.notify_all();
}

void BuildProgress::animate() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop_) {
        if (active_) {
            paint_spinner_locked();
            cv_.wait_for(lock, kFrameInterval, [this] { return stop_ || !active_; });
        } else {
            cv_.wait(lock, [this] { return stop_ || active_; });
        }
    }
}

void BuildProgress::paint_spinner_locked() {
    const std::string_view frame = kSpinnerFrames.at(frame_ % kSpinnerFrames.size());
    ++frame_;
    std::cerr << '\r' << colorize(frame, kCyan) << ' ' << label_ << ' '
              << colorize(format_elapsed(elapsed_seconds_locked()), kDim) << kClearToEol;
    std::cerr.flush();
}

double BuildProgress::elapsed_seconds_locked() const {
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start_;
    return elapsed.count();
}

std::string BuildProgress::colorize(std::string_view text, std::string_view ansi) const {
    if (!color_) {
        return std::string(text);
    }

    std::string result;
    result.reserve(ansi.size() + text.size() + kReset.size());
    result.append(ansi);
    result.append(text);
    result.append(kReset);
    return result;
}

} // namespace dune::cli
