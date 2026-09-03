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
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>

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
        Log() = default;
        ~Log() = default;

        LogLevel view_log_level_ = LogLevel::ATOM_INFO;
        std::mutex log_mutex_;

    public:
        [[nodiscard]] static auto GetLogInstance() -> Log&;

        static auto LogOut(std::string_view channelPrefix, std::string_view channelName, LogLevel level,
                           const std::string& logMessage) -> void;
        static auto SetViewLogLevel(LogLevel viewLogLevel) -> void;

        Log(const Log&) = delete;
        Log& operator=(const Log&) = delete;
};
} // namespace atom

#define LOG_INFO(channel, logMessage)                                                                                  \
    atom::Log::GetLogInstance().LogOut(atom::ResolveChannelPrefix(channel), atom::ResolveChannelName(channel),         \
                                       atom::LogLevel::ATOM_INFO, logMessage)
#define LOG_WARNING(channel, logMessage)                                                                               \
    atom::Log::GetLogInstance().LogOut(atom::ResolveChannelPrefix(channel), atom::ResolveChannelName(channel),         \
                                       atom::LogLevel::ATOM_WARNING, logMessage)
#define LOG_ERROR(channel, logMessage)                                                                                 \
    atom::Log::GetLogInstance().LogOut(atom::ResolveChannelPrefix(channel), atom::ResolveChannelName(channel),         \
                                       atom::LogLevel::ATOM_ERROR, logMessage)
#define LOG_DEBUG(channel, logMessage)                                                                                 \
    atom::Log::GetLogInstance().LogOut(atom::ResolveChannelPrefix(channel), atom::ResolveChannelName(channel),         \
                                       atom::LogLevel::ATOM_DEBUG, logMessage)

#endif // ATOM_LOGSYSTEM_HPP
