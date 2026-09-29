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
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
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

        LogConnection(LogConnection&& other) noexcept : remove_(std::move(other.remove_)) {}
        auto operator=(LogConnection&& other) noexcept -> LogConnection& {
            if (this != &other) {
                Reset();
                remove_ = std::move(other.remove_);
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

class Log {
    private:
        using LogListener = std::function<void(const LogRecord&)>;

        struct LogListenerEntry {
                uint64_t id;
                LogListener listener;
        };

        Log() = default;
        ~Log() = default;

        LogLevel view_log_level_ = LogLevel::ATOM_INFO;
        std::mutex log_mutex_;
        std::vector<LogListenerEntry> listeners_;
        uint64_t next_listener_id_ = 1;

    public:
        [[nodiscard]] static auto GetLogInstance() -> Log&;

        static auto LogOut(std::string_view channelPrefix, std::string_view channelName, LogLevel level,
                           const std::string& logMessage, std::string_view file = {}, int line = 0,
                           std::string_view function = {}) -> void;
        // Configures the process console for UTF-8 text output. This is a
        // no-op outside Windows and must be called explicitly by the host.
        static auto SetConsoleOutputUtf8() -> void;
        static auto SetViewLogLevel(LogLevel viewLogLevel) -> void;
        using Listener = std::function<void(const LogRecord&)>;
        [[nodiscard]] static auto Subscribe(Listener listener) -> LogConnection;
        [[nodiscard]] static auto GetRegisteredChannels() -> std::vector<LogChannelInfo>;

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
