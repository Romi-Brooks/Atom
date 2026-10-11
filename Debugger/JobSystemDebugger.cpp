// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "JobSystemDebugger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>

#include <Concurrency/JobSystem.hpp>

#include <imgui.h>

namespace atom::debugger {
namespace {

// How long a finished job stays visible in the "recent" list.
constexpr double kRecentHoldSeconds = 2.0;
// How many seconds of throughput history the curve shows.
constexpr double kCurveWindowSeconds = 10.0;

} // namespace

auto JobSystemDebugger::OnDrawOverlay() -> void {
    ImGui::Begin("Job System");

    ImGui::Checkbox("Pause", &pause_);

    auto& sys = atom::job::JobSystem::GetInstance();
    const auto running = sys.IsRunning();
    const auto worker_count = sys.WorkerCount();
    const auto stats = sys.Stats();

    ImGui::Text("Workers: %zu", worker_count);
    ImGui::SameLine();
    ImGui::TextColored(running ? ImVec4{0.3f, 1.0f, 0.4f, 1.0f} : ImVec4{1.0f, 0.4f, 0.4f, 1.0f},
                      running ? "running" : "stopped");

    ImGui::Separator();

    // Maintain the rolling window of counters. Samples are appended when the
    // counters move (or once per 0.25 s), so a quiet scheduler shows a flat,
    // honest curve rather than a noisy one.
    const double now = ImGui::GetTime();
    if (!pause_) {
        const bool changed = stats.submitted != last_submitted_ || stats.completed != last_completed_;
        if (changed || samples_.empty() || now - samples_.back().time >= 0.25) {
            samples_.push_back(Sample{now, stats.submitted, stats.completed, stats.pending});
            while (!samples_.empty() && now - samples_.front().time > kCurveWindowSeconds)
                samples_.pop_front();
            last_submitted_ = stats.submitted;
            last_completed_ = stats.completed;
        }
    }

    // Jobs/sec derived from the oldest sample still within a 1 s lookback.
    std::uint64_t submitted_per_sec = 0;
    std::uint64_t completed_per_sec = 0;
    const Sample* oldest = samples_.empty() ? nullptr : &samples_.front();
    if (samples_.size() >= 2) {
        const auto& newest = samples_.back();
        for (const auto& s : samples_) {
            if (newest.time - s.time <= 1.0) {
                oldest = &s;
                break;
            }
        }
        const double span = newest.time - oldest->time;
        if (span > 0.0) {
            submitted_per_sec =
                static_cast<std::uint64_t>((newest.submitted - oldest->submitted) / span);
            completed_per_sec =
                static_cast<std::uint64_t>((newest.completed - oldest->completed) / span);
        }
    }

    ImGui::Text("Submitted:   %llu total, %llu/s",
                static_cast<unsigned long long>(stats.submitted),
                static_cast<unsigned long long>(submitted_per_sec));
    ImGui::Text("Completed:   %llu total, %llu/s",
                static_cast<unsigned long long>(stats.completed),
                static_cast<unsigned long long>(completed_per_sec));
    ImGui::Text("Pending:     %llu", static_cast<unsigned long long>(stats.pending));
    ImGui::Text("Steals:      %llu", static_cast<unsigned long long>(stats.stolen));

    ImGui::Separator();

    // --- Throughput curve: jobs completed per second over the last 10 s ----
    // Most jobs finish inside a single frame, so an instantaneous "pending"
    // count is almost always zero; completed-per-second is the curve that
    // actually shows scheduler activity. Pending is overlaid for reference.
    ImGui::Text("Activity (last %.0f s)", kCurveWindowSeconds);
    throughput_curve_.clear();
    pending_curve_.clear();
    float throughput_max = 16.0f;
    for (std::size_t i = 1; i < samples_.size(); ++i) {
        const auto& prev = samples_[i - 1];
        const auto& curr = samples_[i];
        const double span = curr.time - prev.time;
        const float rate = span > 0.0
            ? static_cast<float>((curr.completed - prev.completed) / span)
            : 0.0f;
        throughput_curve_.push_back(rate);
        pending_curve_.push_back(static_cast<float>(curr.pending));
        throughput_max = std::max(throughput_max, rate);
    }
    char overlay[32]{};
    std::snprintf(overlay, sizeof(overlay), "%llu/s",
                  static_cast<unsigned long long>(completed_per_sec));
    ImGui::PlotLines("##throughput", throughput_curve_.data(),
                     static_cast<int>(throughput_curve_.size()), 0, overlay, 0.0f,
                     throughput_max, ImVec2{-1.0f, 80.0f});
    ImGui::TextDisabled("y: completed jobs/s (max %.0f), x: time", throughput_max);

    ImGui::Separator();

    // --- Currently running jobs (name, or raw pointer address as fallback) ---
    const auto running_jobs = sys.GetRunningJobs();
    ImGui::Text("Running (%zu):", running_jobs.size());
    if (running_jobs.empty()) {
        ImGui::TextDisabled("  (idle)");
    } else {
        for (const auto& rj : running_jobs) {
            char label[96]{};
            if (!rj.name.empty()) {
                std::snprintf(label, sizeof(label), "  %s", std::string{rj.name}.c_str());
            } else {
                std::snprintf(label, sizeof(label), "  <unnamed @%p>",
                              static_cast<const void*>(rj.job));
            }
            ImGui::TextUnformatted(label);
        }
    }

    // --- Recent completions: keeps short-lived jobs visible -----------------
    const auto recent_jobs = sys.GetRecentJobs();
    const auto recent_count = std::count_if(
        recent_jobs.begin(), recent_jobs.end(),
        [&](const auto& r) { return now - r.finished_at <= kRecentHoldSeconds; });
    ImGui::Text("Recent (%zu in last %.0f s):", static_cast<std::size_t>(recent_count),
                kRecentHoldSeconds);
    if (recent_count == 0) {
        ImGui::TextDisabled("  (none)");
    } else {
        ImGui::BeginChild("recent_jobs", ImVec2{0.0f, 120.0f}, true);
        // Newest first so the freshest completion is immediately visible.
        for (auto it = recent_jobs.rbegin(); it != recent_jobs.rend(); ++it) {
            if (now - it->finished_at > kRecentHoldSeconds)
                continue;
            char line[160]{};
            if (!it->name.empty()) {
                std::snprintf(line, sizeof(line), "  %-28s %8.3f ms", it->name.c_str(),
                              it->duration_ms);
            } else {
                std::snprintf(line, sizeof(line), "  %-28s %8.3f ms", "<unnamed>", it->duration_ms);
            }
            ImGui::TextUnformatted(line);
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

} // namespace atom::debugger
