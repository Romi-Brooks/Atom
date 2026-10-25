/**
  * @file           : LogSystem.cpp
  * @brief          : Thread-safe log dispatch, replay ring, and console sink.
  */

#include "LogSystem.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>

#include <Concurrency/ThreadRegistry.hpp>

#ifdef _WIN32
// This implementation file is the only Atom source that needs this API.
// windows.h is never exposed through LogSystem.hpp or included by callers.
// WIN32_LEAN_AND_MEAN provides less macro exposure
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// NOMINMAX means the min/max macros will not be exposed.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif // _WIN32

using atom::Log;
using atom::LogLevel;

namespace {

struct ChannelRegistry {
        std::mutex mutex;
        std::vector<atom::LogChannelInfo> channels;
};

auto GetChannelRegistry() -> ChannelRegistry& {
    static ChannelRegistry registry;
    return registry;
}

auto MakeChannelKey(const atom::LogChannelInfo& channel) -> std::string {
    return channel.prefix + channel.name;
}

// Runtime fallback for the compile-time ATOM_LOG_STRIP_FILE trim. __FILE__ may
// arrive with mixed '/' and '\' separators (or already trimmed). Keep only the
// last directory component plus the file name so console/debugger output shows
// a short, stable "Module/File.cpp" path regardless of the checkout root.
auto TrimSourcePath(const std::string_view path) -> std::string {
    if (path.empty())
        return {};
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string_view::npos)
        return std::string{path};
    const auto prev_slash = path.find_last_of("/\\", slash - 1);
    if (prev_slash == std::string_view::npos)
        return std::string{path.substr(slash + 1)};
    return std::string{path.substr(prev_slash + 1)};
}

} // namespace

namespace atom {

auto RegisterLogChannelDomain(const std::string_view prefix, const std::string_view* names,
                              const std::size_t count) -> void {
    if (!names)
        return;

    auto& registry = GetChannelRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    for (std::size_t i = 0; i < count; ++i) {
        LogChannelInfo channel{std::string{prefix}, std::string{names[i]}};
        const auto key = MakeChannelKey(channel);
        const auto exists = std::ranges::find_if(registry.channels, [&key](const LogChannelInfo& registered) {
            return MakeChannelKey(registered) == key;
        });
        if (exists == registry.channels.end())
            registry.channels.push_back(std::move(channel));
    }
}

} // namespace atom

namespace atom {
auto ToString(const LogLevel level) -> const char* {
    switch (level) {
    case LogLevel::ATOM_DEBUG:
        return "DEBUG";
    case LogLevel::ATOM_INFO:
        return "INFO";
    case LogLevel::ATOM_WARNING:
        return "WARNING";
    case LogLevel::ATOM_ERROR:
        return "ERROR";
    }
    return "UNKNOWN";
}
} // namespace atom

namespace {

// Thread-safe local-time formatting. std::localtime() returns a pointer into a
// shared static buffer and is not reentrant: two threads logging at the same
// moment used to corrupt each other's timestamp (ARCH-110).
auto FormatCurrentTimestamp() -> std::string {
    const auto time = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(time);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count() % 1000;

    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &seconds) != 0)
        return {};
#else
    if (localtime_r(&seconds, &local) == nullptr)
        return {};
#endif

    char buffer[40]{};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03d", local.tm_year + 1900,
                  local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec,
                  static_cast<int>(milliseconds < 0 ? milliseconds + 1000 : milliseconds));
    return buffer;
}

} // namespace

Log::Log() {
    // Console output is drained by a dedicated writer so a realtime/decode
    // thread never blocks on the console and never formats I/O while holding the
    // logger lock. The file sink is drained the same way; it idles until a path
    // is configured.
    console_writer_ = std::thread([this] { ConsoleWriterLoop(); });
    file_writer_ = std::thread([this] { FileWriterLoop(); });
}

Log::~Log() {
    {
        std::lock_guard<std::mutex> lock(console_mutex_);
        console_writer_stop_ = true;
    }
    console_cv_.notify_all();
    if (console_writer_.joinable())
        console_writer_.join();

    {
        std::lock_guard<std::mutex> lock(file_mutex_);
        file_writer_stop_ = true;
    }
    file_cv_.notify_all();
    if (file_writer_.joinable())
        file_writer_.join();

    // Anything the producer queued before the stop request still reaches the
    // console: shutdown must not swallow the final lines. Take the lock even
    // though the writer is gone, so a late producer cannot race this drain.
    {
        std::lock_guard<std::mutex> lock(console_mutex_);
        console_sink_enabled_.store(false, std::memory_order_relaxed);
        for (const auto& line : console_queue_)
            std::cout << line;
        console_queue_.clear();
    }
    std::cout.flush();

    // Drain the file queue the same way, then close the stream.
    {
        std::vector<std::string> pending;
        {
            std::lock_guard<std::mutex> lock(file_mutex_);
            file_sink_enabled_.store(false, std::memory_order_relaxed);
            pending.assign(file_queue_.begin(), file_queue_.end());
            file_queue_.clear();
        }
        std::lock_guard<std::mutex> stream_lock(file_stream_mutex_);
        for (const auto& line : pending) {
            if (file_stream_.is_open())
                file_stream_ << line;
        }
        if (file_stream_.is_open())
            file_stream_.flush();
        file_stream_.close();
    }
}

auto Log::GetLogInstance() -> Log& {
    static Log instance;
    return instance;
}

auto Log::SetConsoleOutputUtf8() -> void {
#ifdef _WIN32
    constexpr unsigned int utf8_code_page = 65001; // Win32 CP_UTF8
    SetConsoleOutputCP(utf8_code_page);
    LOG_INFO(atom::log::core::Logger, "Set the console output code page to UTF-8");
#endif // _WIN32
}

auto Log::FormatConsoleLine(const LogRecord& record) -> std::string {
    std::string line;
    line.reserve(record.timestamp.size() + record.channel_prefix.size() + record.channel_name.size() +
                 record.message.size() + 32);
    line += '[';
    line += record.timestamp;
    line += "] [";
    line += ToString(record.level);
    line += "] ";
    line += record.channel_prefix;
    line += record.channel_name;
    line += " -> ";
    line += record.message;
    if (!record.file.empty()) {
        line += " (";
        line += record.file;
        line += ':';
        line += std::to_string(record.line);
        if (!record.function.empty()) {
            line += " in ";
            line += record.function;
        }
        line += ')';
    }
    line += '\n';
    return line;
}

auto Log::EnqueueConsoleLine(std::string line) -> void {
    if (!console_sink_enabled_.load(std::memory_order_relaxed))
        return;
    {
        std::lock_guard<std::mutex> lock(console_mutex_);
        if (!console_sink_enabled_.load(std::memory_order_relaxed))
            return;
        if (console_queue_capacity_ == 0) {
            // A zero-length queue accepts nothing: drop and count rather than
            // accumulating without bound.
            dropped_console_lines_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // The capacity bounds every pending line, not just the ones still in the
        // deque. The writer pops a line before writing it (it must not hold
        // console_mutex_ across the write), so counting only
        // console_queue_.size() here would let the backlog reach capacity + 1
        // and break GetPendingConsoleLineCount() <= capacity.
        if (console_queue_.size() + console_in_flight_ >= console_queue_capacity_) {
            if (console_queue_.empty()) {
                // The writer holds the only slot. Its line is already being
                // written and cannot be recalled, so the incoming line is the
                // one that has to go.
                dropped_console_lines_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            // Drop the oldest pending line rather than blocking the producer:
            // this path is reachable from audio decode threads.
            console_queue_.pop_front();
            dropped_console_lines_.fetch_add(1, std::memory_order_relaxed);
        }
        console_queue_.push_back(std::move(line));
    }
    console_cv_.notify_one();
}

auto Log::ConsoleWriterLoop() -> void {
    atom::concurrency::ThreadRegistry::ScopedRegistration registration{"LogConsoleWriter"};
    std::unique_lock<std::mutex> lock(console_mutex_);
    while (true) {
        console_cv_.wait(lock, [this] { return console_writer_stop_ || !console_queue_.empty(); });
        if (console_queue_.empty()) {
            if (console_writer_stop_)
                return;
            continue;
        }
        auto line = std::move(console_queue_.front());
        console_queue_.pop_front();
        ++console_in_flight_;
        lock.unlock();
        std::cout << line;
        std::cout.flush();
        lock.lock();
        --console_in_flight_;
        // Wake FlushConsole() promptly once the queue has drained.
        console_cv_.notify_all();
    }
}

auto Log::EnqueueFileLine(std::string line) -> void {
    if (!file_sink_enabled_.load(std::memory_order_relaxed))
        return;
    {
        std::lock_guard<std::mutex> lock(file_mutex_);
        if (!file_sink_enabled_.load(std::memory_order_relaxed))
            return;
        if (file_queue_capacity_ == 0) {
            dropped_file_lines_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (file_queue_.size() + file_in_flight_ >= file_queue_capacity_) {
            if (file_queue_.empty()) {
                dropped_file_lines_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            file_queue_.pop_front();
            dropped_file_lines_.fetch_add(1, std::memory_order_relaxed);
        }
        file_queue_.push_back(std::move(line));
    }
    file_cv_.notify_one();
}

auto Log::FileWriterLoop() -> void {
    atom::concurrency::ThreadRegistry::ScopedRegistration registration{"LogFileWriter"};
    std::unique_lock<std::mutex> lock(file_mutex_);
    while (true) {
        file_cv_.wait(lock, [this] { return file_writer_stop_ || !file_queue_.empty(); });
        if (file_queue_.empty()) {
            if (file_writer_stop_)
                return;
            continue;
        }
        auto line = std::move(file_queue_.front());
        file_queue_.pop_front();
        ++file_in_flight_;
        lock.unlock();

        {
            // The stream is opened/closed by SetFileSinkPath under
            // file_stream_mutex_; serialize with it so a path change never races
            // an in-progress write. file_mutex_ is deliberately NOT held here.
            std::lock_guard<std::mutex> stream_lock(file_stream_mutex_);
            if (file_stream_.is_open()) {
                file_stream_ << line;
                file_stream_.flush();
            }
        }

        lock.lock();
        --file_in_flight_;
        file_cv_.notify_all();
    }
}

auto Log::RecordLocked(const LogRecord& record) -> void {
    if (recent_capacity_ == 0)
        return;
    recent_records_.push_back(record);
    while (recent_records_.size() > recent_capacity_)
        recent_records_.pop_front();
}

auto Log::TakeRecentLocked() const -> std::vector<LogRecord> {
    return {recent_records_.begin(), recent_records_.end()};
}

auto Log::Subscribe(Listener listener) -> atom::LogConnection {
    if (!listener)
        return {};

    auto& instance = GetLogInstance();
    LogListenerPtr shared{};
    std::vector<LogRecord> replay{};
    uint64_t id = 0;
    {
        std::lock_guard<std::mutex> lock(instance.log_mutex_);
        id = instance.next_listener_id_++;
        auto stored = std::make_shared<const LogListener>(std::move(listener));
        shared = stored;
        instance.listeners_.push_back(Log::LogListenerEntry{id, std::move(stored)});
        // ARCH-110: replay what was already logged (resource preparation, asset
        // loading, subsystem init) so a subscriber that attaches late — the
        // LogDebugger, once RenderWindow exists — still sees it.
        replay = instance.TakeRecentLocked();
    }

    // Replay outside the lock: the listener may log again or unsubscribe.
    for (const auto& record : replay)
        (*shared)(record);

    return atom::LogConnection([&instance, id] {
        std::lock_guard<std::mutex> connection_lock(instance.log_mutex_);
        std::erase_if(instance.listeners_, [id](const Log::LogListenerEntry& entry) { return entry.id == id; });
    });
}

auto Log::GetRegisteredChannels() -> std::vector<atom::LogChannelInfo> {
    auto& registry = GetChannelRegistry();
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto channels = registry.channels;
    std::ranges::sort(channels, [](const atom::LogChannelInfo& left, const atom::LogChannelInfo& right) {
        return MakeChannelKey(left) < MakeChannelKey(right);
    });
    return channels;
}

auto Log::LogOut(const std::string_view channelPrefix, const std::string_view channelName, const LogLevel level,
                 std::string logMessage, const std::string_view file, const int line,
                 const std::string_view function) -> void {
    auto& instance = GetLogInstance();

    // The console threshold and the sink switches are atomic, so these
    // decisions never contend with the listener list (ARCH-110). Both sinks
    // share the same view-level threshold and the same formatted line.
    const bool level_passes = level >= instance.view_log_level_.load(std::memory_order_relaxed);
    const bool write_to_console = level_passes && instance.console_sink_enabled_.load(std::memory_order_relaxed);
    const bool write_to_file = level_passes && instance.file_sink_enabled_.load(std::memory_order_relaxed);

    // Build the record outside the lock. FormatCurrentTimestamp() is
    // thread-safe, and `logMessage` is moved rather than copied.
    LogRecord record;
    record.timestamp = FormatCurrentTimestamp();
    record.channel_prefix.assign(channelPrefix);
    record.channel_name.assign(channelName);
    record.level = level;
    record.message = std::move(logMessage);
    record.file = TrimSourcePath(file);
    record.line = line;
    record.function.assign(function);

    // Snapshot the listeners (shared_ptr copies) and append to the replay ring
    // under one short lock.
    std::vector<LogListenerPtr> listeners;
    {
        std::lock_guard<std::mutex> lock(instance.log_mutex_);
        listeners.reserve(instance.listeners_.size());
        for (const auto& entry : instance.listeners_)
            listeners.push_back(entry.listener);
        instance.RecordLocked(record);
    }

    // Hand the formatted line to the console and/or file writer threads. No
    // console/disk I/O and no blocking lock on this thread.
    if (write_to_console || write_to_file) {
        std::string line = FormatConsoleLine(record);
        if (write_to_console)
            instance.EnqueueConsoleLine(line);
        if (write_to_file)
            instance.EnqueueFileLine(std::move(line));
    }

    // Never invoke user/UI callbacks while holding the logger mutex. A callback
    // may safely unsubscribe or emit another log record.
    for (const auto& listener : listeners) {
        if (listener && *listener)
            (*listener)(record);
    }
}

auto Log::SetViewLogLevel(const LogLevel viewLogLevel) -> void {
    // Announce at the current threshold first, then switch: raising the level to
    // ERROR should not silently swallow its own announcement.
    LOG_INFO(atom::log::core::Logger, std::string{"Set log level to "} + ToString(viewLogLevel));
    auto& instance = GetLogInstance();
    instance.view_log_level_.store(viewLogLevel, std::memory_order_relaxed);
}

auto Log::GetViewLogLevel() -> LogLevel {
    return GetLogInstance().view_log_level_.load(std::memory_order_relaxed);
}

auto Log::SetRecentCapacity(const std::size_t capacity) -> void {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex_);
    instance.recent_capacity_ = capacity;
    while (instance.recent_records_.size() > instance.recent_capacity_)
        instance.recent_records_.pop_front();
}

auto Log::GetRecentCapacity() -> std::size_t {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex_);
    return instance.recent_capacity_;
}

auto Log::GetRecentRecords() -> std::vector<LogRecord> {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex_);
    return instance.TakeRecentLocked();
}

auto Log::ClearRecentRecords() -> void {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex_);
    instance.recent_records_.clear();
}

auto Log::SetConsoleSinkEnabled(const bool enabled) -> void {
    auto& instance = GetLogInstance();
    instance.console_sink_enabled_.store(enabled, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(instance.console_mutex_);
        if (!enabled)
            instance.console_queue_.clear();
    }
    instance.console_cv_.notify_all();
}

auto Log::IsConsoleSinkEnabled() -> bool {
    return GetLogInstance().console_sink_enabled_.load(std::memory_order_relaxed);
}

auto Log::SetConsoleQueueCapacity(const std::size_t capacity) -> void {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.console_mutex_);
    instance.console_queue_capacity_ = capacity;
    // The new bound governs the lines that are already pending too: lowering the
    // capacity must not leave the backlog above its bound until the next log
    // call happens to run. Trim to the bound and count what is discarded, like
    // every other drop on this path.
    while (!instance.console_queue_.empty() &&
           instance.console_queue_.size() + instance.console_in_flight_ > capacity) {
        instance.console_queue_.pop_front();
        instance.dropped_console_lines_.fetch_add(1, std::memory_order_relaxed);
    }
}

auto Log::FlushConsole() -> void {
    auto& instance = GetLogInstance();
    std::unique_lock<std::mutex> lock(instance.console_mutex_);
    // Wait for the *writes*, not just the dequeues: the writer unlocks before
    // writing, so an empty queue alone does not mean the output has landed.
    while (!instance.console_queue_.empty() || instance.console_in_flight_ > 0)
        instance.console_cv_.wait_for(lock, std::chrono::milliseconds{1});
}

auto Log::GetPendingConsoleLineCount() -> std::size_t {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.console_mutex_);
    return instance.console_queue_.size() + instance.console_in_flight_;
}

auto Log::GetDroppedConsoleLineCount() -> uint64_t {
    return GetLogInstance().dropped_console_lines_.load(std::memory_order_relaxed);
}

auto Log::EnableTimestampedFileSink(const std::string_view directory, const std::string_view prefix) -> std::string {
    // Empty directory means "next to the working directory"; an empty prefix
    // would produce a nameless "<stamp>.log", so keep a usable default rather
    // than inventing a file nobody can find.
    const std::filesystem::path directory_path =
        directory.empty() ? std::filesystem::path{"."} : std::filesystem::path{std::string{directory}};
    std::error_code ec;
    std::filesystem::create_directories(directory_path, ec);
    if (ec) {
        // Not fatal for logging: write next to the working directory so a
        // read-only tree still produces a log instead of none at all.
        LOG_WARNING(atom::log::core::Main,
                    "Cannot create the log directory '" + directory_path.string() + "': " + ec.message() +
                        "; falling back to the working directory");
        return EnableTimestampedFileSink(".", prefix);
    }

    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &seconds) != 0)
        return {};
#else
    if (localtime_r(&seconds, &local) == nullptr)
        return {};
#endif
    char stamp[32]{};
    std::snprintf(stamp, sizeof(stamp), "%04d%02d%02d-%02d%02d%02d", local.tm_year + 1900, local.tm_mon + 1,
                  local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);

    const std::string stem = prefix.empty() ? std::string{"atom"} : std::string{prefix};
    const auto path = directory_path / (stem + "-" + stamp + ".log");
    if (!SetFileSinkPath(path.string()))
        return {};
    return path.string();
}

auto Log::SetFileSinkPath(const std::string& path) -> bool {
    auto& instance = GetLogInstance();
    bool opened = false;
    {
        std::lock_guard<std::mutex> lock(instance.file_stream_mutex_);
        if (instance.file_stream_.is_open())
            instance.file_stream_.close();
        instance.file_path_.clear();
        if (!path.empty()) {
            instance.file_stream_.open(path, std::ios::out | std::ios::trunc);
            opened = instance.file_stream_.is_open();
            if (opened)
                instance.file_path_ = path;
        }
    }
    // Enable only when a stream is actually open (empty path disables).
    instance.file_sink_enabled_.store(opened, std::memory_order_relaxed);
    return opened;
}

auto Log::GetFileSinkPath() -> std::string {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.file_stream_mutex_);
    return instance.file_path_;
}
auto Log::SetFileSinkEnabled(const bool enabled) -> void {
    auto& instance = GetLogInstance();
    instance.file_sink_enabled_.store(enabled, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(instance.file_mutex_);
        if (!enabled)
            instance.file_queue_.clear();
    }
    instance.file_cv_.notify_all();
}

auto Log::IsFileSinkEnabled() -> bool {
    return GetLogInstance().file_sink_enabled_.load(std::memory_order_relaxed);
}

auto Log::SetFileQueueCapacity(const std::size_t capacity) -> void {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.file_mutex_);
    instance.file_queue_capacity_ = capacity;
    while (!instance.file_queue_.empty() &&
           instance.file_queue_.size() + instance.file_in_flight_ > capacity) {
        instance.file_queue_.pop_front();
        instance.dropped_file_lines_.fetch_add(1, std::memory_order_relaxed);
    }
}

auto Log::FlushFile() -> void {
    auto& instance = GetLogInstance();
    std::unique_lock<std::mutex> lock(instance.file_mutex_);
    while (!instance.file_queue_.empty() || instance.file_in_flight_ > 0)
        instance.file_cv_.wait_for(lock, std::chrono::milliseconds{1});
}

auto Log::GetPendingFileLineCount() -> std::size_t {
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.file_mutex_);
    return instance.file_queue_.size() + instance.file_in_flight_;
}

auto Log::GetDroppedFileLineCount() -> uint64_t {
    return GetLogInstance().dropped_file_lines_.load(std::memory_order_relaxed);
}
