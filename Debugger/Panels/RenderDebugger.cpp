// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "RenderDebugger.hpp"

#include <algorithm>
#include <cstdint>

#include <Runtime/Application.hpp>
#include <Window/RenderWindow.hpp>

#include <imgui.h>

namespace atom::debugger {
namespace {

// Samples kept per curve. At ~140 fps that is a few seconds of history, which is
// enough to see a pattern and short enough that the plot still moves.
constexpr std::size_t kHistorySamples = 480;

} // namespace

auto RenderDebugger::OnDrawOverlay() -> void {
    auto* window = GetTargetWindow();
    auto* device = window != nullptr ? window->GetRenderDevice() : nullptr;

    // Fixed initial width: the verdict and explanation strings change length with
    // the numbers, and an auto-sized window would resize every time they do --
    // moving the widgets under the cursor while someone is reading them.
    ImGui::SetNextWindowSize(ImVec2{340.0f, 0.0f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Render");

    if (device == nullptr) {
        ImGui::TextDisabled("No render device.");
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Pause", &pause_);
    ImGui::Separator();

    DrawFrameBudget();
    ImGui::Separator();
    DrawBackend();

    ImGui::End();
}

auto RenderDebugger::DrawBackend() -> void {
    auto* window = GetTargetWindow();
    auto* device = window != nullptr ? window->GetRenderDevice() : nullptr;
    if (device == nullptr)
        return;

    // `api` is the native API SDL_GPU bound to for this process: the *effective*
    // answer, not the GpuDriver request that produced it.
    const auto& info = device->GetBackendInfo();
    ImGui::Text("Backend: %s", window->GetBackendId().c_str());
    ImGui::Text("API:     %s", info.api.c_str());
    ImGui::TextDisabled("device:  %s", info.device.c_str());
    if (!info.driver_version.empty())
        ImGui::TextDisabled("version: %s", info.driver_version.c_str());
    if (info.driver.empty()) {
        // Only the Vulkan path fills a driver name; D3D12 exposes no equivalent,
        // so say that instead of printing "unknown", which reads like a
        // detection failure on our side.
        ImGui::TextDisabled("driver:  (not reported by this API)");
    } else {
        ImGui::TextDisabled("driver:  %s", info.driver.c_str());
    }
}

auto RenderDebugger::DrawFrameBudget() -> void {
    auto* window = GetTargetWindow();
    auto* device = window != nullptr ? window->GetRenderDevice() : nullptr;
    if (device == nullptr)
        return;

    const auto timing = device->GetFrameTiming();
    const double frame_ms = timing.frame_ms > 0.0 ? timing.frame_ms : 0.0;

    // Every row is always present, even when its value is not meaningful. A row
    // that appears and disappears as the numbers move drags the plots and the
    // backend block up and down with it, which reads as the panel "jumping" and
    // makes the numbers impossible to compare between two frames.
    const bool gpu_period_meaningful = timing.cpu_wait_ms > frame_ms * 0.25 && timing.gpu_period_ms > 0.0;
    ImGui::Text("CPU record  %.3f ms", timing.cpu_encode_ms);
    ImGui::Text("CPU submit  %.3f ms", timing.cpu_submit_ms);
    ImGui::Text("GPU wait    %.3f ms", timing.cpu_wait_ms);
    if (gpu_period_meaningful) {
        ImGui::TextDisabled("GPU period  %.3f ms", timing.gpu_period_ms);
    } else {
        // gpu_period is only meaningful while the CPU actually waits on fences;
        // otherwise it merely mirrors the frame period and reads as "the GPU
        // takes 7 ms" when the GPU is idle.
        ImGui::TextDisabled("GPU period  (idle)");
    }
    ImGui::TextDisabled("frame       %.3f ms (device)", frame_ms);

    // The verdict is the point of this panel: a frame rate alone cannot tell
    // whether the CPU, the GPU or the presentation path is the limiter. `record`
    // and `submit` are split because a blocking present lands in submit, and
    // calling that "the CPU is slow" sends the investigation to the wrong place.
    //
    // Exactly two lines, whatever the case: one verdict, one explanation.
    const double record = timing.cpu_encode_ms;
    const double submit = timing.cpu_submit_ms;
    const char* verdict = "Bound: paced by the display";
    const char* explanation = "Neither the CPU nor the GPU is saturated.";
    ImVec4 verdict_color{0.4f, 1.0f, 0.4f, 1.0f};
    if (timing.cpu_wait_ms > 0.0 && frame_ms > 0.0 && timing.cpu_wait_ms >= frame_ms * 0.5) {
        verdict = "Bound: GPU backpressure";
        explanation = "Half the frame waits on the frame two ago.";
        verdict_color = ImVec4{1.0f, 0.6f, 0.3f, 1.0f};
    } else if (submit > 0.0 && frame_ms > 0.0 && submit >= frame_ms * 0.5) {
        verdict = "Bound: present / swapchain wait";
        explanation = "Submit blocks: compare another API before blaming the engine.";
        verdict_color = ImVec4{1.0f, 0.6f, 0.3f, 1.0f};
    } else if (record + submit >= frame_ms * 0.8 && frame_ms > 0.0) {
        verdict = "Bound: CPU submit path";
        explanation = "No blocking: the engine's own per-frame cost is the limit.";
        verdict_color = ImVec4{1.0f, 0.6f, 0.3f, 1.0f};
    }
    ImGui::TextColored(verdict_color, "%s", verdict);
    ImGui::TextDisabled("%s", explanation);

    if (!pause_) {
        cpu_curve_.push_back(static_cast<float>(record + submit));
        wait_curve_.push_back(static_cast<float>(timing.cpu_wait_ms));
        while (cpu_curve_.size() > kHistorySamples)
            cpu_curve_.erase(cpu_curve_.begin());
        while (wait_curve_.size() > kHistorySamples)
            wait_curve_.erase(wait_curve_.begin());
    }

    float peak = 0.0f;
    for (const float value : cpu_curve_)
        peak = std::max(peak, value);
    for (const float value : wait_curve_)
        peak = std::max(peak, value);
    const float y_max = std::max(peak * 1.2f, 0.1f);

    ImGui::PlotLines("##cpu_curve", cpu_curve_.data(), static_cast<int>(cpu_curve_.size()), 0, "CPU (record+submit)",
                     0.0f, y_max, ImVec2{-1.0f, 56.0f});
    ImGui::PlotLines("##wait_curve", wait_curve_.data(), static_cast<int>(wait_curve_.size()), 0, "GPU wait", 0.0f,
                     y_max, ImVec2{-1.0f, 56.0f});
    ImGui::TextDisabled("y: ms (shared scale), x: frames");

    ImGui::Separator();
    // Where the record window actually went. `screen` is the game's own drawing,
    // `overlay` is the debugger itself -- reported separately so a panel that is
    // expensive becomes visible instead of silently inflating the frame. Both come
    // from the frame loop, not from a log line: this is a number you watch.
    const auto& loop = atom::runtime::Application::GetInstance().GetFrameLoopStats();
    ImGui::Text("screen  %.3f ms", loop.screen_ms);
    ImGui::Text("overlay %.3f ms", loop.overlay_ms);

    const auto phases = device->GetEncoderPhases();
    const double samples = static_cast<double>(device->GetEncoderPhaseFrames() > 0 ? device->GetEncoderPhaseFrames() : 1);
    if (ImGui::CollapsingHeader("Encoder phases (avg/frame)")) {
        ImGui::TextDisabled("2D upload    %.4f ms", phases.encoder_upload_ms / samples);
        ImGui::TextDisabled("2D pass      %.4f ms", phases.encoder_pass_ms / samples);
        ImGui::TextDisabled("2D items     %.4f ms", phases.encoder_items_ms / samples);
        ImGui::TextDisabled("postprocess  %.4f ms", phases.postprocess_ms / samples);
        ImGui::TextDisabled("overlay upload %.4f ms", phases.overlay_upload_ms / samples);
        ImGui::TextDisabled("overlay pass   %.4f ms", phases.overlay_pass_ms / samples);
        ImGui::TextDisabled("overlay draw   %.4f ms", phases.overlay_draw_ms / samples);
        ImGui::TextDisabled("ImGui::Render  %.4f ms", phases.overlay_build_ms / samples);
        ImGui::TextDisabled("%llu segments, %llu items, %llu overlay verts",
                            static_cast<unsigned long long>(phases.segments / static_cast<std::uint64_t>(samples)),
                            static_cast<unsigned long long>(phases.items / static_cast<std::uint64_t>(samples)),
                            static_cast<unsigned long long>(phases.overlay_vertices /
                                                            static_cast<std::uint64_t>(samples)));
        ImGui::TextDisabled("pp targets %llu, textures +%llu/-%llu over %d frames",
                            static_cast<unsigned long long>(phases.postprocess_targets),
                            static_cast<unsigned long long>(phases.texture_creates),
                            static_cast<unsigned long long>(phases.texture_destroys),
                            device->GetEncoderPhaseFrames());
    }
}

} // namespace atom::debugger
