// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "FpsDebugger.hpp"

#include <algorithm>
#include <cstdio>

#include <Debugger/PanelLayout.hpp>

#include <imgui.h>

namespace atom::debugger {
namespace {

// 曲线窗口：保留最近多少秒的帧率历史。
constexpr double kHistorySeconds = 10.0;

} // namespace

auto FpsDebugger::OnDrawOverlay() -> void {
    if (!slot_applied_) {
        ApplyFpsPanelSlot();
        slot_applied_ = true;
    }

    ImGui::Begin("FPS");

    ImGui::Checkbox("Pause", &pause_);

    // ImGui 的 Framerate 是 60 帧指数平滑后的帧率，稳定且与旧示例一致。
    const float fps = ImGui::GetIO().Framerate;
    const float frame_ms = fps > 0.0f ? 1000.0f / fps : 0.0f;
    ImGui::Text("FPS: %.1f", static_cast<double>(fps));
    ImGui::SameLine();
    ImGui::TextDisabled("(%.2f ms)", static_cast<double>(frame_ms));

    // 每帧采一个点，滚动窗口只保留最近 kHistorySeconds 秒。
    const double now = ImGui::GetTime();
    if (!pause_) {
        samples_.push_back(Sample{now, fps});
        while (!samples_.empty() && now - samples_.front().time > kHistorySeconds) {
            samples_.pop_front();
        }
    }

    // 窗口内的 min / avg / max，用于直观判断帧率波动幅度。
    float min_fps = fps;
    float max_fps = fps;
    double sum = 0.0;
    for (const Sample& s : samples_) {
        min_fps = std::min(min_fps, s.fps);
        max_fps = std::max(max_fps, s.fps);
        sum += static_cast<double>(s.fps);
    }
    const float avg_fps = samples_.empty() ? fps
                                           : static_cast<float>(sum / static_cast<double>(samples_.size()));

    ImGui::Separator();
    ImGui::Text("min %.0f   avg %.0f   max %.0f", static_cast<double>(min_fps), static_cast<double>(avg_fps),
                static_cast<double>(max_fps));

    // 帧率曲线。y 轴从 0 到「窗口最大值上浮一点」，掉帧会表现为明显的凹陷。
    fps_curve_.clear();
    fps_curve_.reserve(samples_.size());
    for (const Sample& s : samples_) {
        fps_curve_.push_back(s.fps);
    }
    char overlay[32]{};
    std::snprintf(overlay, sizeof(overlay), "%.0f", static_cast<double>(fps));
    const float y_max = std::max(max_fps * 1.1f, 60.0f);
    ImGui::PlotLines("##fps_curve", fps_curve_.data(), static_cast<int>(fps_curve_.size()), 0, overlay, 0.0f,
                     y_max, ImVec2{-1.0f, 80.0f});
    ImGui::TextDisabled("y: FPS (last %.0f s), x: time", kHistorySeconds);

    ImGui::End();
}

} // namespace atom::debugger
