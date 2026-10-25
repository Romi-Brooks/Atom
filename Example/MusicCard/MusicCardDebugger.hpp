/**
 * @file           : MusicCardDebugger.hpp
 * @brief          : ImGui debugger panel for the MusicCard example: frame
 *                   pacing, backend switching, and playback controls.
 *
 * Split out of the original single-file MusicCard.cpp.
 */

#ifndef ATOM_EXAMPLE_MUSIC_CARD_DEBUGGER_HPP
#define ATOM_EXAMPLE_MUSIC_CARD_DEBUGGER_HPP

#include "MusicCardScreen.hpp"

#include <string>

#include <Backend/Contracts/Audio/AudioBackendId.hpp>
#include <Debugger/DebugPanel.hpp>
#include <Display/DisplaySystem.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Audio/AudioBackend.hpp>
#include <Window/RenderWindow.hpp>

#include <imgui.h>

class MusicCardDebugger final : public atom::debugger::DebugPanel {
    public:
        explicit MusicCardDebugger(MusicCardScreen& screen) : DebugPanel("MusicCardDebugger"), screen_{screen} {}

    protected:
        auto OnDrawOverlay() -> void override {
            ImGui::Begin("Music Card Debugger");
            // Presentation policy lives in DisplaySystem now, not on the window:
            // show the applied mode (what the swapchain actually runs) instead of
            // a two-state "vsync on/off", which could not express Mailbox.
            const auto& display = atom::display::DisplaySystem::GetInstance();
            ImGui::Text("Present: %s", std::string{atom::render::ToString(display.Applied().present_mode)}.c_str());
            if (!reported_frame_pacing_ && ImGui::GetIO().Framerate > 0.0f) {
                LOG_INFO(atom::log::core::Window,
                         "Music card debugger frame pacing: " + std::to_string(ImGui::GetIO().Framerate) +
                             " FPS, present " +
                             std::string{atom::render::ToString(display.Applied().present_mode)});
                reported_frame_pacing_ = true;
            }
            ImGui::Separator();

            ImGui::Text("Active backend: %s (generation %llu)", atom::audio::GetAudioBackendId().c_str(),
                        static_cast<unsigned long long>(atom::audio::GetAudioBackendGeneration()));

            if (ImGui::Button("Use native SDL3")) {
                atom::audio::SetAudioBackend(atom::backend::AudioBackendId::Sdl3);
            }

            ImGui::SameLine();
            if (ImGui::Button("Use SDL3_mixer")) {
                atom::audio::SetAudioBackend(atom::backend::AudioBackendId::Sdl3Mixer);
            }

            if (ImGui::Button("Switch corner")) {
                screen_.ToggleCorner();
            }
            ImGui::SameLine();
            if (ImGui::Button(screen_.IsCardVisible() ? "Hide card" : "Show card")) {
                screen_.ToggleCardVisibility();
            }

            ImGui::Separator();
            ImGui::Text("State: %s", screen_.GetAnimationStateName().data());
            ImGui::Text("Position: %s", screen_.GetCornerName().data());
            ImGui::Text("Playback: %s", screen_.IsPlaying() ? "playing" : "stopped");

            const auto& playback = screen_.GetPlaybackInfo();
            if (!playback.id.empty()) {
                if (playback.duration > 0.0f)
                    ImGui::Text("Time: %.1f / %.1f s", static_cast<double>(playback.position),
                                static_cast<double>(playback.duration));
                else
                    ImGui::Text("Time: %.1f s (duration unknown)", static_cast<double>(playback.position));
            }

            if (screen_.HasTracks()) {
                const auto track = screen_.GetCurrentDisplay();
                ImGui::Text("Title: %s", track.title.c_str());
                ImGui::Text("Artist: %s", track.artist.c_str());
                ImGui::Text("Audio: %s", track.is_loaded ? "loaded" : "visual demo only");
                ImGui::Text("Metadata: %s", track.metadata_loaded ? "resolved" : "pending");
                ImGui::Text("Artwork: %s%s%s (%zu bytes)", track.artwork_bytes == 0 ? "fallback" : "embedded",
                            track.artwork_mime_type.empty() ? "" : ", ", track.artwork_mime_type.c_str(),
                            track.artwork_bytes);
            }

            ImGui::Separator();
            ImGui::TextDisabled("Keyboard: Left/Right, Space, C, H, Esc");
            ImGui::End();
        }

    private:
        MusicCardScreen& screen_;
        bool reported_frame_pacing_ = false;
};

#endif // ATOM_EXAMPLE_MUSIC_CARD_DEBUGGER_HPP
