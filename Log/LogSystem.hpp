/**
* @file           : LogSystem.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_LOGSYSTEM_HPP
#define ATOM_LOGSYSTEM_HPP

// Standard Library
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
//	using reference: LogSystem.md

// 通道域定义机制（ATOM_DEFINE_CHANNELS 等预处理基础设施）
#include "LogChannelMacros.hpp"

// 引擎通道域：Atom 内置通道（新增/修改通道请编辑 AtomLogChannels.hpp）
#include "AtomLogChannels.hpp"

namespace atom {
enum class LogLevel {
    ATOM_DEBUG,   // 0
    ATOM_INFO,    // 1
    ATOM_WARNING, // 2
    ATOM_ERROR    // 3
};

// Stable display name for a level (also used by the debugger and the console
// sink, so the mapping lives in exactly one place).
[[nodiscard]] auto ToString(LogLevel level) -> const char*;

struct LogRecord {
    std::string timestamp;
    std::string channel_prefix;
    std::string channel_name;
    LogLevel level = LogLevel::ATOM_INFO;
    std::string message;
    std::string file;      // __FILE__ of the call site (empty when unspecified)
    int line = 0;          // __LINE__ of the call site (0 when unspecified)
    std::string function;  // __FUNCTION__ of the call site (empty when unspecified)
};

struct LogChannelInfo {
    std::string prefix;
    std::string name;
};

class LogConnection {
    public:
        LogConnection() = default;
        ~LogConnection() {
            Reset();
        }

        LogConnection(LogConnection&& other) noexcept : remove_(std::exchange(other.remove_, nullptr)) {}
        auto operator=(LogConnection&& other) noexcept -> LogConnection& {
            if (this != &other) {
                Reset();
                // exchange() so the moved-from connection is deterministically
                // disconnected: a moved-from std::function is not required to be
                // empty, and resetting it would unsubscribe the moved-to one.
                remove_ = std::exchange(other.remove_, nullptr);
            }
            return *this;
        }
        LogConnection(const LogConnection&) = delete;
        auto operator=(const LogConnection&) -> LogConnection& = delete;

        auto Reset() noexcept -> void {
            if (remove_) {
                remove_();
                remove_ = nullptr;
            }
        }

        [[nodiscard]] auto IsConnected() const noexcept -> bool {
            return static_cast<bool>(remove_);
        }

    private:
        friend class Log;
        explicit LogConnection(std::function<void()> remove) : remove_(std::move(remove)) {}

        std::function<void()> remove_;
};

// 通用通道解析
// ADL
[[nodiscard]] constexpr auto ResolveChannelPrefix(const std::string_view) -> std::string_view {
    return {};
}
template <typename TChannel>
    requires std::is_enum_v<TChannel>
[[nodiscard]] constexpr auto ResolveChannelPrefix(const TChannel channel) -> std::string_view {
    return GetChannelPrefix(channel);
}

[[nodiscard]] constexpr auto ResolveChannelName(const std::string_view channelName) -> std::string_view {
    return channelName;
}
template <typename TChannel>
    requires std::is_enum_v<TChannel>
[[nodiscard]] constexpr auto ResolveChannelName(const TChannel channel) -> std::string_view {
    return GetChannelName(channel);
}

// Process-wide log dispatcher.
//
// Threading model (ARCH-110):
//  - `view_log_level_` is atomic, so the console threshold can be read from any
//    thread (including audio decode threads) without a lock.
//  - `log_mutex_` protects the subscriber list and the replay ring only. User
//    callbacks always run *after* it is released, so a subscriber may log again
//    or unsubscribe from inside its callback. Delivery is synchronous, so a
//    callback must not re-log every record it receives (that recurses).
//  - `console_mutex_` protects the console queue. Console output is drained by a
//    dedicated writer thread, so a realtime/decode thread never blocks on the
//    console (a Windows console write can be very slow) and never formats I/O
//    while holding the logger lock.
//  - `LogOut` is safe to call concurrently from any thread.
class Log {
    public:
        // Recent-log replay ring: how many records Subscribe() replays.
        static constexpr std::size_t default_recent_capacity = 512;
        // Console queue bound. It bounds the *pending* lines — queued plus the
        // one the writer has popped and is writing — so
        // GetPendingConsoleLineCount() <= capacity always holds. When it is full
        // the oldest pending line is dropped rather than blocking the producer
        // (or the incoming line, when the writer already holds the only slot);
        // 0 accepts nothing.
        static constexpr std::size_t default_console_queue_capacity = 4096;

    private:
        using LogListener = std::function<void(const LogRecord&)>;
        // Held through shared_ptr so LogOut snapshots listeners by bumping a
        // refcount instead of deep-copying every std::function on the hot path.
        using LogListenerPtr = std::shared_ptr<const LogListener>;

        struct LogListenerEntry {
                uint64_t id;
                LogListenerPtr listener;
        };

        Log();
        ~Log();

        // Replay ring: the most recent records, replayed to every new subscriber
        // so the startup/asset-loading phase is not lost before the debugger
        // overlay attaches.
        auto RecordLocked(const LogRecord& record) -> void;
        auto TakeRecentLocked() const -> std::vector<LogRecord>;

        // Console sink internals.
        auto ConsoleWriterLoop() -> void;
        auto EnqueueConsoleLine(std::string line) -> void;
        [[nodiscard]] static auto FormatConsoleLine(const LogRecord& record) -> std::string;

        std::atomic<LogLevel> view_log_level_{LogLevel::ATOM_INFO};

        mutable std::mutex log_mutex_;
        std::vector<LogListenerEntry> listeners_;
        uint64_t next_listener_id_ = 1;
        std::deque<LogRecord> recent_records_{};
        std::size_t recent_capacity_ = default_recent_capacity;

        std::mutex console_mutex_;
        std::condition_variable console_cv_;
        std::deque<std::string> console_queue_{};
        std::size_t console_queue_capacity_ = default_console_queue_capacity;
        // Lines the writer has popped but not finished writing. FlushConsole()
        // must wait for this to reach zero, otherwise it can return while the
        // last line is still in the writer's local.
        std::size_t console_in_flight_ = 0;
        std::thread console_writer_;
        bool console_writer_stop_ = false;
        // Atomic so LogOut can skip formatting entirely when the sink is off
        // without touching console_mutex_.
        std::atomic<bool> console_sink_enabled_{true};
        std::atomic<uint64_t> dropped_console_lines_{0};

    public:
        [[nodiscard]] static auto GetLogInstance() -> Log&;

        // `logMessage` is taken by value so the caller's temporary (a literal, a
        // concatenation, or a std::string) is moved into the record instead of
        // being copied again.
        static auto LogOut(std::string_view channelPrefix, std::string_view channelName, LogLevel level,
                           std::string logMessage, std::string_view file = {}, int line = 0,
                           std::string_view function = {}) -> void;
        // Configures the process console for UTF-8 text output. This is a
        // no-op outside Windows and must be called explicitly by the host.
        static auto SetConsoleOutputUtf8() -> void;
        static auto SetViewLogLevel(LogLevel viewLogLevel) -> void;
        [[nodiscard]] static auto GetViewLogLevel() -> LogLevel;
        using Listener = std::function<void(const LogRecord&)>;
        // Registers `listener` and replays the buffered records to it
        // immediately. The replay happens outside the logger lock, so the
        // listener may log or (un)subscribe from inside the callback.
        //
        // Delivery is synchronous: a listener that logs unconditionally on every
        // record recurses, so callbacks must not re-log the record they receive
        // without a filter. Reset() removes the subscription but does not wait
        // for an in-flight callback on another thread; a subscriber that owns
        // state must hand the callback a weak_ptr/shared_ptr to it (the
        // LogDebugger does exactly this).
        [[nodiscard]] static auto Subscribe(Listener listener) -> LogConnection;
        [[nodiscard]] static auto GetRegisteredChannels() -> std::vector<LogChannelInfo>;

        // --- recent-record ring (ARCH-110 startup log loss) -----------------
        // 0 disables buffering; lowering the capacity trims the buffer.
        static auto SetRecentCapacity(std::size_t capacity) -> void;
        [[nodiscard]] static auto GetRecentCapacity() -> std::size_t;
        [[nodiscard]] static auto GetRecentRecords() -> std::vector<LogRecord>;
        static auto ClearRecentRecords() -> void;

        // --- console sink ---------------------------------------------------
        // Disabling the sink stops console output but never affects listeners.
        static auto SetConsoleSinkEnabled(bool enabled) -> void;
        [[nodiscard]] static auto IsConsoleSinkEnabled() -> bool;
        // Bounds the pending-line queue. `0` is the degenerate bound: the sink
        // accepts nothing and counts every line as dropped. The queue is always
        // bounded, because this path is reachable from audio/render threads that
        // must never block or grow memory without limit. Lowering the bound also
        // trims the lines already pending, counting them as dropped.
        static auto SetConsoleQueueCapacity(std::size_t capacity) -> void;
        // Blocks until every queued console line has been *written* (not merely
        // dequeued). Intended for tools, tests and shutdown paths, never for a
        // realtime thread.
        static auto FlushConsole() -> void;
        // Lines still queued or in the writer's hand.
        [[nodiscard]] static auto GetPendingConsoleLineCount() -> std::size_t;
        // Lines dropped because the console queue was full. Monotonic: it is
        // never reset, so read it as a delta between two samples.
        [[nodiscard]] static auto GetDroppedConsoleLineCount() -> uint64_t;

        Log(const Log&) = delete;
        Log& operator=(const Log&) = delete;
};
} // namespace atom

// Trim the source-root prefix from __FILE__ at compile time so call-site
// paths stay short and stable across checkouts. Set ATOM_SOURCE_ROOT_PREFIX to
// your repository's canonical root (e.g. "D:/Project/Repo/Atom/") and the
// trailing path separator is handled here; otherwise the full __FILE__ is kept.
#ifndef ATOM_SOURCE_ROOT_PREFIX
#define ATOM_LOG_STRIP_FILE(file) file
#else
#define ATOM_LOG_STRIP_FILE(file) (ATOM_SOURCE_ROOT_PREFIX "" file + sizeof(ATOM_SOURCE_ROOT_PREFIX) - 1)
#endif

#define LOG_INFO(channel, logMessage)                                                                                  \
    atom::Log::GetLogInstance().LogOut(atom::ResolveChannelPrefix(channel), atom::ResolveChannelName(channel),         \
                                       atom::LogLevel::ATOM_INFO, logMessage, ATOM_LOG_STRIP_FILE(__FILE__), __LINE__, \
                                       __FUNCTION__)
#define LOG_WARNING(channel, logMessage)                                                                               \
    atom::Log::GetLogInstance().LogOut(atom::ResolveChannelPrefix(channel), atom::ResolveChannelName(channel),         \
                                       atom::LogLevel::ATOM_WARNING, logMessage, ATOM_LOG_STRIP_FILE(__FILE__),        \
                                       __LINE__, __FUNCTION__)
#define LOG_ERROR(channel, logMessage)                                                                                 \
    atom::Log::GetLogInstance().LogOut(atom::ResolveChannelPrefix(channel), atom::ResolveChannelName(channel),         \
                                       atom::LogLevel::ATOM_ERROR, logMessage, ATOM_LOG_STRIP_FILE(__FILE__), __LINE__, \
                                       __FUNCTION__)
#define LOG_DEBUG(channel, logMessage)                                                                                 \
    atom::Log::GetLogInstance().LogOut(atom::ResolveChannelPrefix(channel), atom::ResolveChannelName(channel),         \
                                       atom::LogLevel::ATOM_DEBUG, logMessage, ATOM_LOG_STRIP_FILE(__FILE__), __LINE__, \
                                       __FUNCTION__)

#endif // ATOM_LOGSYSTEM_HPP
