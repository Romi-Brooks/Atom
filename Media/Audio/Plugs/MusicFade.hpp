/**
  * @file           : FadeSwitch.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_MUSICFADE_HPP
#define ATOM_MUSICFADE_HPP

// Standard Library
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <string>

// Forward declarations
namespace atom {
    class Music;
}

namespace atom::audio {
	enum class FadeState {
		Idle,
		FadingOut,
		FadingIn,
		Completed
	};

	class MusicFade {
		private:
			struct FadeContext {
				std::string fromId;
				std::string toId;
				float duration{0.0f};
				float progress{0.0f};
				FadeState state{FadeState::Idle};
			} context_;

			std::atomic<bool> stop_requested_{false};

			mutable std::mutex mutex_;

			std::thread fade_thread_;

			std::function<void(FadeState, const std::string&, const std::string&)> callback_;

			Music& music_;

			auto FadeProcess() -> void;

		public:
			explicit MusicFade(Music& music)
				: music_(music) {}

			~MusicFade();

			auto Switch(const std::string& toId, float duration) -> bool;
			auto Stop() -> void;

			auto SetCallback(const std::function<void(FadeState, const std::string&, const std::string&)>& callback) -> void;

			[[nodiscard]] auto IsFading() const -> bool;

			[[nodiscard]] auto GetState() const -> FadeState;
			[[nodiscard]] auto GetProgress() const -> float;
			[[nodiscard]] auto GetFromId() const -> std::string;
			[[nodiscard]] auto GetToId() const -> std::string;
			[[nodiscard]] auto GetDuration() const -> float;
		};
}

#endif // ATOM_MUSICFADE_HPP
