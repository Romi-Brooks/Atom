/**
  * @file           : SFX.cpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/14
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#include <ranges>
#include <algorithm>

#include <Log/LogSystem.hpp>
#include <Media/Audio/SFX/Manager/SFXManager.hpp>
#include <Media/Audio/Manager/VolumeManager.hpp>
#include <Media/Audio/Backend/SDL3SFXSource.hpp>

#include "SFX.hpp"

namespace atom {

// ---------------------------------------------------------------------------
// Voice-pool helpers
// ---------------------------------------------------------------------------

auto SFX::ReapFinishedVoices(SFXEntry& entry) -> void {
    int reaped = 0;
    for (auto& voice : entry.voices) {
        if (voice && voice->GetState() == AudioSourceState::Playing && voice->IsFinished()) {
            voice->Stop();
            ++reaped;
        }
    }
    if (reaped > 0) {
        LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_SFX,
                  "Reaped " + std::to_string(reaped) + " finished voice(s)");
    }
}

auto SFX::AllocateVoice(SFXEntry& entry) -> IAudioSource* {
    // 1. Try to find an already-Stopped voice
    for (auto& voice : entry.voices) {
        if (voice && voice->GetState() == AudioSourceState::Stopped) {
            LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_SFX,
                      "Reusing stopped voice (" + std::to_string(entry.voices.size()) + " total)");
            return voice.get();
        }
    }

    // 2. Still under the limit — create a new voice
    if (entry.voices.size() < entry.maxVoices) {
        auto new_voice = std::make_unique<SDL3SFXSource>();
        new_voice->SetBuffer(entry.pcmData.data(),
                             static_cast<uint32_t>(entry.pcmData.size()));
        new_voice->SetSpec(entry.spec);
        auto* ptr = new_voice.get();
        entry.voices.push_back(std::move(new_voice));
        LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_SFX,
                  "Created new voice (" + std::to_string(entry.voices.size()) + "/" +
                  std::to_string(entry.maxVoices) + ")");
        return ptr;
    }

    // 3. All voices busy — steal the oldest one (round-robin replacement)
    auto* target = entry.voices.front().get();
    LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX,
                "All voices busy — stealing oldest voice");
    target->Stop();
    if (auto* sdl = dynamic_cast<SDL3SFXSource*>(target)) {
        sdl->SetBuffer(entry.pcmData.data(),
                       static_cast<uint32_t>(entry.pcmData.size()));
        sdl->SetSpec(entry.spec);
    }
    std::rotate(entry.voices.begin(), entry.voices.begin() + 1, entry.voices.end());
    return target;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

auto SFX::Load(const std::string& id, const std::string& filePath) -> bool {
    auto& sfx_manager = SFXManager::GetManager();
    if (!sfx_manager.LoadSFXFiles(id, filePath)) {
        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Error when loading SFX: " + id);
        return false;
    }

    const auto buffer = sfx_manager.GetSFXBuffer(id);
    if (!buffer) {
        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Error getting buffer for SFX: " + id);
        return false;
    }

    auto entry = std::make_unique<SFXEntry>();
    const auto* pcm_bytes = reinterpret_cast<const uint8_t*>(buffer->GetSamples());
    const auto byte_count = buffer->GetSampleCount() * SDL_AUDIO_BYTESIZE(buffer->GetFormat());
    entry->pcmData.assign(pcm_bytes, pcm_bytes + byte_count);

    entry->spec.format = static_cast<SDL_AudioFormat>(buffer->GetFormat());
    entry->spec.channels = buffer->GetChannelCount();
    entry->spec.freq = static_cast<int>(buffer->GetSampleRate());

    LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_SFX,
              "Stored entry: fmt=" + std::to_string(entry->spec.format) +
              " freq=" + std::to_string(entry->spec.freq) +
              " ch=" + std::to_string(entry->spec.channels) +
              " data=" + std::to_string(entry->pcmData.size()) + " bytes");

    entries_[id] = std::move(entry);
    return true;
}

auto SFX::Play(const std::string& id) -> void {
    const auto it = entries_.find(id);
    if (it == entries_.end() || !it->second) {
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "Play: unknown id '" + id + "'");
        return;
    }

    auto& entry = *it->second;
    ReapFinishedVoices(entry);

    auto* voice = AllocateVoice(entry);
    if (!voice) {
        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Failed to allocate voice for: " + id);
        return;
    }

    voice->SetVolume(VolumeManager::GetInstance().GetEffectiveSfxVolume());
    voice->Play();
    LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX, "Playing: " + id);
}

auto SFX::Stop(const std::string& id) -> void {
    const auto it = entries_.find(id);
    if (it == entries_.end() || !it->second) {
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "Stop: unknown id '" + id + "'");
        return;
    }

    int stopped = 0;
    for (auto& voice : it->second->voices) {
        if (voice) { voice->Stop(); ++stopped; }
    }
    LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_SFX,
              "Stopped " + std::to_string(stopped) + " voice(s) for: " + id);
}

auto SFX::StopAll() -> void {
    int total = 0;
    for (auto& [_, entry] : entries_) {
        if (!entry) continue;
        for (auto& voice : entry->voices) {
            if (voice) { voice->Stop(); ++total; }
        }
    }
    LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_SFX, "StopAll: stopped " + std::to_string(total) + " voice(s)");
}

auto SFX::SetVolume(const std::string& id, const float volume) -> void {
    const auto it = entries_.find(id);
    if (it == entries_.end() || !it->second) {
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "SetVolume: unknown id '" + id + "'");
        return;
    }

    for (auto& voice : it->second->voices) {
        if (voice) voice->SetVolume(volume);
    }
    LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_SFX,
              "Set volume=" + std::to_string(volume) + " for: " + id);
}

auto SFX::Play(const std::string& id, const float volume) -> void {
    const auto it = entries_.find(id);
    if (it == entries_.end() || !it->second) {
        LOG_WARNING(atom::LogChannel::ATOM_AUDIO_SFX, "Play: unknown id '" + id + "'");
        return;
    }

    auto& entry = *it->second;
    ReapFinishedVoices(entry);

    auto* voice = AllocateVoice(entry);
    if (!voice) {
        LOG_ERROR(atom::LogChannel::ATOM_AUDIO_SFX, "Failed to allocate voice for: " + id);
        return;
    }

    voice->SetVolume(volume);
    voice->Play();
    LOG_INFO(atom::LogChannel::ATOM_AUDIO_SFX,
             "Playing: " + id + " at volume " + std::to_string(volume));
}

auto SFX::IsLoaded(const std::string& id) const -> bool {
    return entries_.contains(id) && entries_.at(id) != nullptr;
}

auto SFX::GetSound(const std::string& id) -> IAudioSource* {
    const auto it = entries_.find(id);
    if (it == entries_.end() || !it->second) return nullptr;

    auto& entry = *it->second;
    for (auto& voice : entry.voices) {
        if (voice && !voice->IsFinished()) return voice.get();
    }
    return entry.voices.empty() ? nullptr : entry.voices.front().get();
}

auto SFX::Reset() -> void {
    LOG_DEBUG(atom::LogChannel::ATOM_AUDIO_SFX,
              "Reset: clearing " + std::to_string(entries_.size()) + " entry(s)");
    StopAll();
    entries_.clear();
}

} // namespace atom
