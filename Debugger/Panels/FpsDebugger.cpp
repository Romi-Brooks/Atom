// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "FpsDebugger.hpp"

#include <algorithm>
#include <cstdio>

#include <Debugger/PanelLayout.hpp>

#include <imgui.h>

namespace atom::debugger {
namespace {

// How much frame-rate history to keep on screen.
constexpr double kHistorySeconds = 10.0;

} // namespace

auto FpsDebugger::OnDrawOverlay() -> void {
    if (!slot_applied_) {
        ApplyFpsPanelSlot();
        slot_applied_ = true;
    }

    ImGui::Begin("FPS");

    ImGui::Checkbox("Pause", &pause_);

    // ImGui's Framerate is a 60-frame exponential smoothing: stable enough to
    // read at 2500 fps, where the raw per-frame value would flicker.
    const float fps = ImGui::GetIO().Framerate;
    const float frame_ms = fps > 0.0f ? 1000.0f / fps : 0.0f;
    // The unsmoothed frame time: "one slow frame" vs "sustained slow" is the
    // difference between a hitch and a bottleneck.
    const float dt_ms = ImGui::GetIO().DeltaTime * 1000.0f;
    ImGui::Text("FPS: %.1f", static_cast<double>(fps));
    ImGui::SameLine();
    ImGui::TextDisabled("(%.2f ms)", static_cast<double>(frame_ms));

    const double now = ImGui::GetTime();
    if (!pause_) {
        samples_.push_back(Sample{now, fps, dt_ms});
        while (!samples_.empty() && now - samples_.front().time > kHistorySeconds)
            samples_.pop_front();
    }

    float min_fps = fps;
    float max_fps = fps;
    float max_dt_ms = dt_ms;
    double sum = 0.0;
    for (const Sample& sample : samples_) {
        min_fps = std::min(min_fps, sample.fps);
        max_fps = std::max(max_fps, sample.fps);
        max_dt_ms = std::max(max_dt_ms, sample.dt_ms);
        sum += static_cast<double>(sample.fps);
    }
    const float avg_fps =
        samples_.empty() ? fps : static_cast<float>(sum / static_cast<double>(samples_.size()));

    ImGui::Separator();
    ImGui::Text("min %.0f   avg %.0f   max %.0f", static_cast<double>(min_fps), static_cast<double>(avg_fps),
                static_cast<double>(max_fps));
    ImGui::Text("Frame: %.2f ms   spike %.2f ms (last %.0f s)", static_cast<double>(dt_ms),
                static_cast<double>(max_dt_ms), kHistorySeconds);

    // The curve; y starts at 0 so a dip is visible as a dip, not as noise.
    fps_curve_.clear();
    fps_curve_.reserve(samples_.size());
    for (const Sample& sample : samples_)
        fps_curve_.push_back(sample.fps);
    char overlay[32]{};
    std::snprintf(overlay, sizeof(overlay), "%.0f", static_cast<double>(fps));
    ImGui::PlotLines("##fps_curve", fps_curve_.data(), static_cast<int>(fps_curve_.size()), 0, overlay, 0.0f,
                     std::max(max_fps * 1.1f, 60.0f), ImVec2{-1.0f, 80.0f});
    ImGui::TextDisabled("y: FPS (last %.0f s), x: time", kHistorySeconds);

    ImGui::End();
}

} // namespace atom::debugger
