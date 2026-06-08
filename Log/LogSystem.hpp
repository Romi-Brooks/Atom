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

//	using reference: LogSystem.md

namespace atom {
	enum class LogLevel {
		ATOM_INFO,
		ATOM_WARNING,
		ATOM_ERROR,
		ATOM_DEBUG
	};

	class LogChannel {
	public:
		// User can create custom channels directly
		explicit LogChannel(std::string channelName)
			: channelName_(std::move(channelName))
			, displayString_(channelName_ + " -> ") {}

		[[nodiscard]] const std::string& GetChannelName() const { return channelName_; }
		[[nodiscard]] const std::string& GetDisplayString() const { return displayString_; }

		// Pre-defined engine channels
		static const LogChannel ATOM_ENTITY;
		static const LogChannel ATOM_ENTITY_NPC;
		static const LogChannel ATOM_ENTITY_PLAYER;

		static const LogChannel ATOM_CONFIG_MOVEMENT;

		static const LogChannel ATOM_FILESYSTEM;

		static const LogChannel ATOM_MAIN;

		static const LogChannel ATOM_LUA;

		static const LogChannel ATOM_AUDIO_MUSIC;
		static const LogChannel ATOM_AUDIO_SFX;
		static const LogChannel ATOM_AUDIO_PLUG_MUSICFADE;

		static const LogChannel ATOM_VIDEO;

		static const LogChannel ATOM_UTILITIES_PACKAGER;

		static const LogChannel ATOM_WINDOW;
		static const LogChannel ATOM_SCREEN;
		static const LogChannel ATOM_SCREEN_MANAGER;

	private:
		std::string channelName_;
		std::string displayString_;

		// Private constructor for pre-defined channels with custom display string format
		LogChannel(std::string channelName, std::string displayString)
			: channelName_(std::move(channelName))
			, displayString_(std::move(displayString)) {}
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
