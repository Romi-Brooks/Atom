/**
  * @file           : LogSystem.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <string>
#include <iostream>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>

// Self Dependencies
#include "LogSystem.hpp"

using atom::Log;
using atom::LogLevel;
using atom::LogChannel;

// Forward Function
auto GetLogLevel(const LogLevel& logLevel) -> std::string {
    switch (logLevel) {
        case LogLevel::ATOM_INFO: return "INFO";
        case LogLevel::ATOM_WARNING: return"WARNING";
        case LogLevel::ATOM_ERROR: return "ERROR";
        case LogLevel::ATOM_DEBUG: return "DEBUG";
    }
	return "Error log level";
}

auto GetLogChannel(const LogChannel& channel) -> std::string {
	switch (channel) {
		case LogChannel::ATOM_ENTITY:return "ATOM.Entity -> ";
		case LogChannel::ATOM_ENTITY_NPC: return "ATOM.Entity.NPC -> ";
		case LogChannel::ATOM_ENTITY_PLAYER: return "ATOM.Entity.Player -> ";

		case LogChannel::ATOM_CONFIG_MOVEMENT: return "ATOM.Movement -> ";

		case LogChannel::ATOM_FILESYSTEM: return "ATOM.Filesystem -> ";

		case LogChannel::ATOM_LOGGER: return "ATOM.Logger -> ";

		case LogChannel::ATOM_LUA: return "ATOM.Lua -> ";

		case LogChannel::ATOM_AUDIO_MUSIC: return "ATOM.Audio.Music -> ";
		case LogChannel::ATOM_AUDIO_SFX: return"ATOM.Audio.SFX -> ";
		case LogChannel::ATOM_AUDIO_PLUG_MUSICFADE: return "ATOM.Audio.Plug.MusicFade -> ";

		case LogChannel::ATOM_VIDEO: return"ATOM.Video -> ";

		case LogChannel::ATOM_WINDOW: return "ATOM.Window -> ";
		case LogChannel::ATOM_SCREEN: return  "ATOM.Screen -> ";
		case LogChannel::ATOM_SCREEN_MANAGER: return "ATOM.Screen.Manager -> ";

		case LogChannel::ATOM_UTILITIES_PACKAGER: return "ATOM.Utilities.Packager -> ";

		case LogChannel::GAME_NPC: return "Game.NPC -> ";
		case LogChannel::GAME_PLAYER: return "Game.Player -> ";
		case LogChannel::GAME_SCREEN: return "Game.Screen -> ";

		case LogChannel::GAME_MAIN: return "Game.Main -> ";
	}
	return "Call func with error channel.";
}
auto GetCurrentTime() -> std::stringstream {
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

auto Log::LogOut(const LogChannel channel, const LogLevel level, const std::string& logMessage) -> void {
    std::lock_guard<std::mutex> lock(GetLogInstance().log_mutex_);

	const std::string FullLogMessage = "[" + GetCurrentTime().str() + "] [" + GetLogLevel(level) + "] " + GetLogChannel(channel) + logMessage;
    std::cout << FullLogMessage << std::endl;
}

auto Log::SetViewLogLevel(const LogLevel viewLogLevel) -> void {
    GetLogInstance().view_log_level_ = viewLogLevel;
}
