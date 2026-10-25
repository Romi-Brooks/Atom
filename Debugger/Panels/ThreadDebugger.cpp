// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include "ThreadDebugger.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

#include <Concurrency/ThreadRegistry.hpp>

#include <imgui.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif // _WIN32

namespace atom::debugger {
namespace {

// Sampling interval for the "recent CPU" delta. Short enough to feel live,
// long enough that the delta is not dominated by timer noise at ~600 fps.
constexpr double kSampleIntervalSeconds = 0.5;

auto SteadySeconds() -> double {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Cumulative CPU time (kernel + user) for a thread handle, in milliseconds.
auto ReadCpuMilliseconds(void* handle) -> double {
#ifdef _WIN32
    if (handle == nullptr)
        return 0.0;
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    if (!GetThreadTimes(static_cast<HANDLE>(handle), &creation, &exit, &kernel, &user))
        return 0.0;
    const auto to_100ns = [](const FILETIME& ft) -> std::uint64_t {
        return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    };
    return static_cast<double>(to_100ns(kernel) + to_100ns(user)) / 10000.0;
#else
    (void)handle;
    return 0.0;
#endif
}

} // namespace

ThreadDebugger::~ThreadDebugger() {
#ifdef _WIN32
    if (main_handle_)
        CloseHandle(static_cast<HANDLE>(main_handle_));
#endif
    main_handle_ = nullptr;
}

auto ThreadDebugger::EnsureMainHandle() -> void {
#ifdef _WIN32
    if (main_handle_)
        return;
    // The panel draws on the render thread, so GetCurrentThread() is that
    // thread; duplicate its pseudo-handle so we can read its CPU time here.
    HANDLE duplicated = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &duplicated, 0, FALSE,
                        DUPLICATE_SAME_ACCESS)) {
        main_handle_ = duplicated;
    }
#else
    (void)main_handle_;
#endif
}

auto ThreadDebugger::OnDrawOverlay() -> void {
    ImGui::Begin("Threads");

    ImGui::Checkbox("Pause", &pause_);
    ImGui::SameLine();
    ImGui::TextDisabled("delta over last %.1f s", kSampleIntervalSeconds);

    EnsureMainHandle();
    auto& registry = atom::concurrency::ThreadRegistry::GetInstance();
    const auto snapshot = registry.Snapshot();

    // One reading per live thread, plus the render/main thread we are on.
    struct Reading {
            std::thread::id id;
            std::string name;
            bool alive = false;
            double cpu_ms = 0.0;
    };
    std::vector<Reading> readings;
    readings.reserve(snapshot.size() + 1);
    readings.push_back(Reading{std::this_thread::get_id(), "Main/Render", true, ReadCpuMilliseconds(main_handle_)});
    for (const auto& s : snapshot)
        readings.push_back(Reading{s.id, s.name, s.alive, s.cpu_ms});

    const double now = SteadySeconds();
    const bool do_sample = !pause_ && (display_.empty() || now - last_sample_ >= kSampleIntervalSeconds);
    if (do_sample) {
        display_.clear();
        display_.reserve(readings.size());
        for (const auto& r : readings) {
            Row row;
            row.name = r.name;
            row.alive = r.alive;
            row.total_ms = r.cpu_ms;
            const auto it = prev_cpu_.find(r.id);
            row.delta_ms = (it != prev_cpu_.end()) ? (r.cpu_ms - it->second) : 0.0;
            display_.push_back(std::move(row));
        }
        prev_cpu_.clear();
        for (const auto& r : readings)
            prev_cpu_[r.id] = r.cpu_ms;
        last_sample_ = now;
        // Heaviest thread first so the regression suspect is on top.
        std::sort(display_.begin(), display_.end(),
                  [](const Row& a, const Row& b) { return a.delta_ms > b.delta_ms; });
    }

    ImGui::Text("Registered: %zu thread(s) + main", snapshot.size());
    double total_delta = 0.0;
    for (const auto& row : display_)
        total_delta += row.delta_ms;

    ImGui::Separator();
    ImGui::Text("  %-24s %10s %10s %8s", "thread", "delta ms", "total ms", "share");
    ImGui::Separator();
    for (const auto& row : display_) {
        const double share = total_delta > 0.0 ? row.delta_ms / total_delta * 100.0 : 0.0;
        const bool heavy = share >= 20.0 && total_delta > 0.0;
        const ImVec4 color = !row.alive  ? ImVec4{0.6f, 0.6f, 0.6f, 1.0f}
                             : heavy      ? ImVec4{0.95f, 0.4f, 0.35f, 1.0f}
                                          : ImVec4{0.85f, 0.85f, 0.85f, 1.0f};
        char line[128]{};
        std::snprintf(line, sizeof(line), "  %-24s %9.2f %9.0f %7.1f%%", row.name.c_str(), row.delta_ms,
                      row.total_ms, share);
        ImGui::TextColored(color, "%s", line);
    }
    if (display_.empty())
        ImGui::TextDisabled("  (no threads registered)");

    ImGui::End();
}

} // namespace atom::debugger
