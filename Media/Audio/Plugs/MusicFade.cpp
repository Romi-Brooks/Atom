/**
  * @file           : FadeSwitch.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

// Standard Library
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

// Engine Headers
#include <Log/LogSystem.hpp>
#include <Media/Audio/Music/Music.hpp>
#include <Media/Audio/Manager/VolumeManager.hpp>

// Self Dependency
#include "MusicFade.hpp"

namespace atom::audio {
	using FadeCallback = std::function<void(FadeState, const std::string&, const std::string&)>;

	MusicFade::~MusicFade() {
		Stop();
		// Ensure the thread ends correctly
		// 确保线程正确结束
		if (fade_thread_.joinable()) {
			try {
				fade_thread_.join();
			}
			catch (...) {
				LOG_ERROR(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Error when destructing the music entity.");
			}
		}
	}

	auto MusicFade::Switch(const std::string& toId, const float duration) -> bool {
	    std::lock_guard<std::mutex> lock(mutex_);
	    // 1. Stop ongoing fade transition
	    // 1. 停止正在进行的淡入淡出
	    if (context_.state != FadeState::Idle) {
	        stop_requested_ = true;
	        if (fade_thread_.joinable()) {
	            try {
	                fade_thread_.join();
	            }
	            catch (const std::exception& e) {
	                LOG_ERROR(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Join old fade thread failed: " + std::string(e.what()));
	            }
	        }
	        stop_requested_ = false;
	        context_.state = FadeState::Idle;
	        fade_thread_ = std::thread{};
	    }

	    // 2. Get the currently playing music as source
	    // 2. 获取当前播放的音乐作为源
		const std::string fromId = music_.GetNowPlaying();

	    // 3. Check if music is loaded
	    // 3. 音乐是否已加载
	    if (!music_.IsLoaded(toId)) {
	        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Cannot switch music: target music not loaded: " + toId);
	        return false;
	    }

	    // 4. If no music is playing, start playing target music directly
	    // 4. 如果当前没有音乐在播放，直接开始播放目标音乐
	    if (fromId.empty()) {
	        LOG_INFO(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "No current music, starting playback of: " + toId);
	        music_.Play(toId);
	        return true;
	    }

	    // 5. If source and target music are the same, no switch needed
	    // 5. 如果源音乐和目标音乐相同，不需要切换
	    if (fromId == toId) {
	        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Source and target music are the same: " + fromId);
	        return true;
	    }

	    // 6. Check if target music is loaded
	    // 6. 目标音乐是否已加载
	    if (!music_.IsLoaded(fromId)) {
	        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Cannot switch music: current playing music is not loaded: " + fromId);
	        return false;
	    }

	    // 7. Ensure thread object is clean
	    // 7. 确保线程对象是干净的
	    if (fade_thread_.joinable()) {
	        fade_thread_.join();
	        fade_thread_ = std::thread{};
	    }

	    // 8. Initialize context
	    // 8. 初始化上下文
	    context_.fromId = fromId;
	    context_.toId = toId;
	    context_.duration = duration;
	    context_.progress = 0.0f;
	    context_.state = FadeState::FadingOut;

	    // 9. Start new fade thread
	    // 9. 启动新交换线程
	    try {
	        fade_thread_ = std::thread(&MusicFade::FadeProcess, this);
	        LOG_INFO(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Switching music from " + fromId + " to " + toId);
	    }
	    catch (const std::exception& e) {
	        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Failed to start fade thread: " + std::string(e.what()));
	        context_.state = FadeState::Idle;
	        return false;
	    }

	    // 10. Callback notification
	    // 10. 回调通知
	    if (callback_) {
	        callback_(FadeState::FadingOut, fromId, toId);
	    }

	    return true;
	}

	// 停止
	auto MusicFade::Stop() -> void {
	    std::lock_guard<std::mutex> lock(mutex_);
	    if (context_.state == FadeState::Idle) {
	        return; // No running thread
	    }

	    // Trigger stop request
	    // 触发停止请求
	    stop_requested_ = true;

	    // Wait for thread to finish
	    // 等待线程结束
	    if (fade_thread_.joinable()) {
	        try {
	            fade_thread_.join();
	        } catch (const std::exception& e) {
	            LOG_ERROR(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Join fade thread failed when stop: " + std::string(e.what()));
	        }
	    }

	    // Reset state
	    // 重置状态
	    context_.state = FadeState::Idle;
	    stop_requested_ = false;
		fade_thread_ = std::thread{};
	    LOG_INFO(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Fade switch stopped");
	}

	// Set callback
	// 设置回调
	auto MusicFade::SetCallback(const FadeCallback& callback) -> void {
	    std::lock_guard<std::mutex> lock(mutex_);
	    callback_ = callback;
	}

	// Get state
	// 获取状态
	auto MusicFade::GetState() const -> FadeState {
	    std::lock_guard<std::mutex> lock(mutex_);
	    return context_.state;
	}

	// Get progress
	// 获取进度
	auto MusicFade::GetProgress() const -> float {
	    std::lock_guard<std::mutex> lock(mutex_);
	    return context_.progress;
	}

	// Get source music ID
	// 获取源音乐ID
	auto MusicFade::GetFromId() const -> std::string {
	    std::lock_guard<std::mutex> lock(mutex_);
	    return context_.fromId;
	}

	// Get target music ID
	// 获取目标音乐ID
	auto MusicFade::GetToId() const -> std::string {
	    std::lock_guard<std::mutex> lock(mutex_);
	    return context_.toId;
	}

	// Get duration
	// 获取持续时间
	auto MusicFade::GetDuration() const -> float {
	    std::lock_guard<std::mutex> lock(mutex_);
	    return context_.duration;
	}

	// Check if fade is in progress
	// 判断是否正在淡入淡出
	auto MusicFade::IsFading() const -> bool {
	    std::lock_guard<std::mutex> lock(mutex_);
	    return context_.state != FadeState::Idle;
	}

	auto MusicFade::FadeProcess() -> void {
	    try {
	        const float peakVolume = music_.GetMusicVolume();
	        constexpr int steps = 100;
	        int stepDuration = 0;
	        std::string fromId, toId;

	        // 1. Get context info
	        // 1.获取上下文信息
	        {
	            std::lock_guard<std::mutex> lock(mutex_);
	            stepDuration = static_cast<int>((context_.duration * 1000) / (steps * 2));
	            fromId = context_.fromId;
	            toId = context_.toId;
	        }

	        LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Starting fade process: " + fromId + " -> " + toId);

	        // 2. Fade-out phase
	        // 2.淡出阶段
	        for (int i = 0; i <= steps; ++i) {
	            if (stop_requested_) {
	                std::lock_guard<std::mutex> lock(mutex_);
	                context_.state = FadeState::Idle;
	                return;
	            }

	            const float volume = peakVolume * (1.0f - static_cast<float>(i) / steps);
	            music_.SetVolume(fromId, volume);

	            {
	                std::lock_guard<std::mutex> lock(mutex_);
	                context_.progress = static_cast<float>(i) / (steps * 2);
	            }
	            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(stepDuration)));
	        }

	        // 3. Switch music
	        // 3.切换音乐
	        if (!stop_requested_) {
	            music_.Stop(fromId);
	            music_.Play(toId, 0.0f);
	            music_.SetNowPlaying(toId); // Ensure current playing state is updated

	            {
	                std::lock_guard<std::mutex> lock(mutex_);
	                context_.state = FadeState::FadingIn;
	                if (callback_) {
	                    callback_(FadeState::FadingIn, fromId, toId);
	                }
	            }
	        }

	        // 4. Fade-in phase
	        // 4.淡入阶段
	        for (int i = 0; i <= steps; ++i) {
	            if (stop_requested_) {
	                music_.Stop(toId);
	                music_.ClearNowPlaying();
	                std::lock_guard<std::mutex> lock(mutex_);
	                context_.state = FadeState::Idle;
	                return;
	            }

	            const float volume = peakVolume * (static_cast<float>(i) / steps);
	            music_.SetVolume(toId, volume);

	            {
	                std::lock_guard<std::mutex> lock(mutex_);
	                context_.progress = 0.5f + (static_cast<float>(i) / (steps * 2));
	            }

	            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(stepDuration)));
	        }

	        // 5. Completion phase
	        // 5.完成阶段
	        if (!stop_requested_) {
	            std::lock_guard<std::mutex> lock(mutex_);
	            context_.state = FadeState::Completed;
	            if (callback_) {
	                callback_(FadeState::Completed, fromId, toId);
	            }
	            LOG_INFO(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Switching music completed: " + fromId + " -> " + toId);

	            context_.state = FadeState::Idle;
	        }
	    }
		    catch (const std::exception& e) {
		        std::lock_guard<std::mutex> lock(mutex_);
		        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Exception in fade process: " + std::string(e.what()));
		        context_.state = FadeState::Idle;
		        music_.ClearNowPlaying();
		    }
		    catch (...) {
		        std::lock_guard<std::mutex> lock(mutex_);
		        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_PLUG_MUSICFADE, "Unknown exception in fade process");
		        context_.state = FadeState::Idle;
		        music_.ClearNowPlaying();
		    }
	}
}
