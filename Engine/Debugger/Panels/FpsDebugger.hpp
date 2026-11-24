// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file FpsDebugger.hpp
 * @brief Frame-rate readout with a scrolling history.
 *
 * Answers "how fast", nothing else: current/smoothed frame rate, the frame time
 * it implies, a min/avg/max over a rolling window, the worst instant in that
 * window, and the curve. Where the frame time goes (CPU vs GPU) and what
 * presentation mode produced it are separate panels on purpose -- a panel that
 * shows everything is read by nobody.
 */

#ifndef ATOM_DEBUGGER_FPS_DEBUGGER_HPP
#define ATOM_DEBUGGER_FPS_DEBUGGER_HPP

#include <cstddef>
#include <deque>
#include <vector>

#include <Debugger/DebugPanel.hpp>

namespace atom::debugger {

class FpsDebugger final : public DebugPanel {
    public:
        FpsDebugger() : DebugPanel("FpsDebugger") {}

    protected:
        auto OnDrawOverlay() -> void override;

    private:
        // One sample per frame. `dt_ms` is the unsmoothed frame time, which is
        // what tells a single hitch apart from a sustained slow frame.
        struct Sample {
                double time = 0.0;
                float fps = 0.0f;
                float dt_ms = 0.0f;
        };

        std::deque<Sample> samples_;
        // Flattened scratch buffer rebuilt each frame for ImGui::PlotLines.
        std::vector<float> fps_curve_;
        bool pause_ = false;
        bool slot_applied_ = false;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_FPS_DEBUGGER_HPP
