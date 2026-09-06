/**
  * @file           : LogSystem.cpp
  * @brief          : Thread-safe console output and log subscriptions.
  */

#include "LogSystem.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

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

static auto GetLogLevel(const LogLevel& logLevel) -> std::string {
    switch (logLevel) {
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

static auto FormatCurrentTimestamp() -> std::string {
    const auto time = std::chrono::system_clock::now();
    const auto time_t = std::chrono::system_clock::to_time_t(time);
    std::stringstream time_string;
    time_string << std::put_time(std::localtime(&time_t), "%Y-%m-%d %X");
    return time_string.str();
}

auto Log::GetLogInstance() -> Log& {
    static Log instance;
    return instance;
}

auto Log::SetConsoleOutputUtf8() -> void {
#ifdef _WIN32
    constexpr unsigned int utf8_code_page = 65001; // Win32 CP_UTF8
    SetConsoleOutputCP(utf8_code_page);
#endif // _WIN32
}

auto Log::Subscribe(Listener listener) -> atom::LogConnection {
    if (!listener)
        return {};

    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex_);
    const auto id = instance.next_listener_id_++;
    instance.listeners_.push_back(Log::LogListenerEntry{id, std::move(listener)});

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
                 const std::string& logMessage) -> void {
    auto& instance = GetLogInstance();
    LogRecord record;
    std::vector<Listener> listeners;
    bool write_to_console = false;

    {
        std::lock_guard<std::mutex> lock(instance.log_mutex_);

        // Subscribers receive every record. The console view level is only a
        // console concern, so LogDebugger can apply its own filters.
        record.timestamp = FormatCurrentTimestamp();
        record.channel_prefix = channelPrefix;
        record.channel_name = channelName;
        record.level = level;
        record.message = logMessage;
        write_to_console = level >= instance.view_log_level_;

        listeners.reserve(instance.listeners_.size());
        for (const auto& entry : instance.listeners_)
            listeners.push_back(entry.listener);

        if (write_to_console) {
            std::cout << "[" << record.timestamp << "] [" << GetLogLevel(level) << "] " << record.channel_prefix
                      << record.channel_name << " -> " << record.message << std::endl;
        }
    }

    // Never invoke user/UI callbacks while holding the logger mutex. A
    // callback may safely unsubscribe or emit another log record.
    for (const auto& listener : listeners) {
        if (listener)
            listener(record);
    }
}

auto Log::SetViewLogLevel(const LogLevel viewLogLevel) -> void {
    LOG_INFO(atom::core::LogChannel::LOGGER, "Set log level to " + GetLogLevel(viewLogLevel));
    auto& instance = GetLogInstance();
    std::lock_guard<std::mutex> lock(instance.log_mutex_);
    instance.view_log_level_ = viewLogLevel;
}
