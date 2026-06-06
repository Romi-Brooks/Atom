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

namespace atom {
	enum class LogLevel {
		ATOM_INFO,
		ATOM_WARNING,
		ATOM_ERROR,
		ATOM_DEBUG
	};
	enum class LogChannel {
		ATOM_ENTITY,
		ATOM_ENTITY_NPC,
		ATOM_ENTITY_PLAYER,

		ATOM_CONFIG_MOVEMENT,

		ATOM_FILESYSTEM,

		ATOM_LOGGER,

		ATOM_LUA,

		ATOM_AUDIO_MUSIC,
		ATOM_AUDIO_SFX,
		ATOM_AUDIO_PLUG_MUSICFADE,

		ATOM_VIDEO,

		ATOM_UTILITIES_PACKAGER,

		ATOM_WINDOW,
		ATOM_SCREEN,
		ATOM_SCREEN_MANAGER,

		GAME_NPC,
		GAME_PLAYER,
		GAME_SCREEN,

		GAME_MAIN,
	};

	class Log {
	private:
		Log() = default;
		~Log() = default;


		LogLevel view_log_level_ = LogLevel::ATOM_INFO;
		std::mutex log_mutex_;

	public:
		[[nodiscard]] static auto GetLogInstance() -> Log&;

		static auto LogOut(LogChannel channel, LogLevel level, const std::string& logMessage) -> void;
		static auto SetViewLogLevel(LogLevel viewLogLevel) -> void;

		Log(const Log&) = delete;
		Log& operator=(const Log&) = delete;
	};
}

#define LOG_INFO(channel,logMessage) atom::Log::GetLogInstance().LogOut(channel, atom::LogLevel::ATOM_INFO, logMessage)
#define LOG_WARNING(channel,logMessage) atom::Log::GetLogInstance().LogOut(channel, atom::LogLevel::ATOM_WARNING, logMessage)
#define LOG_ERROR(channel,logMessage) atom::Log::GetLogInstance().LogOut(channel, atom::LogLevel::ATOM_ERROR, logMessage)
#define LOG_DEBUG(channel,logMessage) atom::Log::GetLogInstance().LogOut(channel, atom::LogLevel::ATOM_DEBUG, logMessage)

#endif // ATOM_LOGSYSTEM_HPP
