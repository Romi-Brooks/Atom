// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file JobSystemDebugger.hpp
 * @brief ImGui diagnostic panel for the Atom job scheduler.
 *
 * Shows a live view of scheduler throughput: jobs submitted/completed per
 * second, a quantity-over-time curve, the jobs currently running, and a short
 * history of recent completions with their durations (most jobs finish far
 * faster than a frame, so the "running" list alone would be almost always
 * empty).
 */

#ifndef ATOM_DEBUGGER_JOB_SYSTEM_DEBUGGER_HPP
#define ATOM_DEBUGGER_JOB_SYSTEM_DEBUGGER_HPP

#include <cstdint>
#include <deque>
#include <vector>

#include <Debugger/DebugPanel.hpp>

namespace atom::debugger {

class JobSystemDebugger final : public DebugPanel {
    public:
        JobSystemDebugger() : DebugPanel("JobSystemDebugger") {}

    protected:
        auto OnDrawOverlay() -> void override;

    private:
        // One sample of the scheduler's counters at a point in time. The rolling
        // window feeds both the per-second figures and the throughput curve.
        struct Sample {
                double time = 0.0;
                std::uint64_t submitted = 0;
                std::uint64_t completed = 0;
                std::uint64_t pending = 0;
        };

        std::deque<Sample> samples_;
        // Flat arrays backing ImGui::PlotLines; rebuilt each frame from samples_.
        std::vector<float> throughput_curve_;
        std::vector<float> pending_curve_;
        std::uint64_t last_submitted_ = 0;
        std::uint64_t last_completed_ = 0;
        bool pause_ = false;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_JOB_SYSTEM_DEBUGGER_HPP
