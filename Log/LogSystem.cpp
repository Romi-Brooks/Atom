/**
  * @file           : LogSystem.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Self Dependency
#include "LogSystem.hpp"

// Standard Library
#include <iostream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

using atom::Log;
using atom::LogLevel;

// Forward Function
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
    return "Error log level";
}

static auto GetCurrentTime() -> std::stringstream {
    const auto Time = std::chrono::system_clock::now();
    const auto TimeT = std::chrono::system_clock::to_time_t(Time);
    std::stringstream TimeString;
    TimeString << std::put_time(std::localtime(&TimeT), "%Y-%m-%d %X");
    return TimeString;
}

auto Log::GetLogInstance() -> Log& {
    static Log LogInstance;
    return LogInstance;
}

auto Log::LogOut(const std::string_view channelPrefix, const std::string_view channelName, const LogLevel level,
                 const std::string& logMessage) -> void {
    auto& instance = GetLogInstance();

    // only output logs at or above the current view level
    if (level < instance.view_log_level_) {
        return;
    }

    std::lock_guard<std::mutex> lock(instance.log_mutex_);

    std::string FullLogMessage = "[" + GetCurrentTime().str() + "] [" + GetLogLevel(level) + "] ";
    FullLogMessage += channelPrefix;
    FullLogMessage += channelName;
    FullLogMessage += " -> ";
    FullLogMessage += logMessage;
    std::cout << FullLogMessage << std::endl;
}

auto Log::SetViewLogLevel(const LogLevel viewLogLevel) -> void {
    LOG_INFO(atom::core::LogChannel::LOGGER, "Set log level to " + GetLogLevel(viewLogLevel));
    GetLogInstance().view_log_level_ = viewLogLevel;
}
