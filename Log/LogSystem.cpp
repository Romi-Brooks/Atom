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
#include <string>
#include <iostream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

using atom::Log;
using atom::LogChannel;
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
namespace atom {
    // Pre-defined engine channel constants
    const LogChannel LogChannel::ATOM_ENTITY("Atom.Entity", "Atom.Entity -> ");
    const LogChannel LogChannel::ATOM_ENTITY_NPC("Atom.Entity.NPC", "Atom.Entity.NPC -> ");
    const LogChannel LogChannel::ATOM_ENTITY_PLAYER("Atom.Entity.Player", "Atom.Entity.Player -> ");

    const LogChannel LogChannel::ATOM_CONFIG_MOVEMENT("Atom.Movement", "Atom.Movement -> ");

    const LogChannel LogChannel::ATOM_FILESYSTEM("Atom.Filesystem", "Atom.Filesystem -> ");

    const LogChannel LogChannel::ATOM_LOGGER("Atom.Logger", "Atom.Logger -> ");

    const LogChannel LogChannel::ATOM_MAIN("Atom.Main", "Atom.Main -> ");

    const LogChannel LogChannel::ATOM_LUA("Atom.Lua", "Atom.Lua -> ");

    const LogChannel LogChannel::ATOM_AUDIO_MUSIC("Atom.Audio.Music", "Atom.Audio.Music -> ");
    const LogChannel LogChannel::ATOM_AUDIO_SFX("Atom.Audio.SFX", "Atom.Audio.SFX -> ");
    const LogChannel LogChannel::ATOM_AUDIO_PLUG_MUSICFADE("Atom.Audio.Plug.MusicFade", "Atom.Audio.Plug.MusicFade -> ");
    const LogChannel LogChannel::ATOM_AUDIO_MINIMP3("Atom.Audio.Minimp3", "Atom.Audio.Minimp3 -> ");
    const LogChannel LogChannel::ATOM_AUDIO_WAVPROF("Atom.Audio.WavProf", "Atom.Audio.WavProf -> ");

    const LogChannel LogChannel::ATOM_BACKEND_RUNTIME("Atom.Backend.Runtime", "Atom.Backend.Runtime -> ");

    const LogChannel LogChannel::ATOM_VIDEO("Atom.Video", "Atom.Video -> ");

    const LogChannel LogChannel::SDL_BACKEND_AUDIO("SDL.Backend.Audio", "SDL.Backend.Audio -> ");
    const LogChannel LogChannel::SDL_BACKEND_VIDEO("SDL.Backend.Video", "SDL.Backend.Video -> ");
    const LogChannel LogChannel::SDL_BACKEND_RENDER("SDL.Backend.Render", "SDL.Backend.Render -> ");
    const LogChannel LogChannel::SDL_BACKEND_WINDOW("SDL.Backend.Window", "SDL.Backend.Window -> ");

    const LogChannel LogChannel::ATOM_WINDOW("Atom.Window", "Atom.Window -> ");
    const LogChannel LogChannel::ATOM_SCREEN("Atom.Screen", "Atom.Screen -> ");
    const LogChannel LogChannel::ATOM_SCREEN_MANAGER("Atom.Screen.Manager", "Atom.Screen.Manager -> ");

    const LogChannel LogChannel::ATOM_UTILITIES_PACKAGER("Atom.Utilities.Packager", "Atom.Utilities.Packager -> ");
}

static auto GetLogChannel(const LogChannel& channel) -> std::string {
    return channel.GetDisplayString();
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

auto Log::LogOut(const LogChannel& channel, const LogLevel level, const std::string& logMessage) -> void {
    auto& instance = GetLogInstance();

    // only output logs at or above the current view level
    if (level < instance.view_log_level_) {
        return;
    }

    std::lock_guard<std::mutex> lock(instance.log_mutex_);

    const std::string FullLogMessage =
        "[" + GetCurrentTime().str() + "] [" + GetLogLevel(level) + "] " + GetLogChannel(channel) + logMessage;
    std::cout << FullLogMessage << std::endl;
}

auto Log::SetViewLogLevel(const LogLevel viewLogLevel) -> void {
    LOG_INFO(LogChannel::ATOM_LOGGER, "Set log level to " + GetLogLevel(viewLogLevel));
    GetLogInstance().view_log_level_ = viewLogLevel;
}
