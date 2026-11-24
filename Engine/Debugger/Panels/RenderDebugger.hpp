// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file RenderDebugger.hpp
 * @brief What the renderer runs on, and where a frame's time goes.
 *
 * Two questions that belong together but never mix with "how fast": the device
 * identity (backend / API / adapter / driver / version) and the split between
 * the engine's own submit cost and the time it spends blocked on the GPU. That
 * split is what separates "the CPU cannot feed the GPU" from "the GPU is behind",
 * which is the first thing any frame-rate investigation needs and the one thing
 * a frame counter cannot answer.
 *
 * There is no gpu_busy figure here on purpose: SDL_GPU exposes no timestamp
 * queries, so no backend can know how long its shaders ran. The fence-derived
 * numbers below are the honest substitute.
 */

#ifndef ATOM_DEBUGGER_RENDER_DEBUGGER_HPP
#define ATOM_DEBUGGER_RENDER_DEBUGGER_HPP

#include <cstddef>
#include <vector>

#include <Debugger/DebugPanel.hpp>

namespace atom::debugger {

class RenderDebugger final : public DebugPanel {
    public:
        RenderDebugger() : DebugPanel("RenderDebugger") {}

    protected:
        auto OnDrawOverlay() -> void override;

    private:
        auto DrawBackend() -> void;
        auto DrawFrameBudget() -> void;

        // Rolling history for the two curves, one entry per drawn frame.
        std::vector<float> cpu_curve_;
        std::vector<float> wait_curve_;
        bool pause_ = false;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_RENDER_DEBUGGER_HPP
