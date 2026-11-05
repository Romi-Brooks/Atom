// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file FpsDebugger.hpp
 * @brief 可 attach 的帧率诊断面板（is-a DebugPanel）。
 *
 * 取代各 example 里临时手写的 `ImGui::Text("FPS: ...")`：一个滚动窗口保存最近
 * 若干秒的帧率历史，用 ImGui::PlotLines 画出曲线，让帧率抖动/掉帧一眼可见，
 * 而不只是一个即时数字。
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
        // 某一时刻的帧率采样（ImGui 已做 60 帧指数平滑，曲线更稳定）。
        // dt_ms 记录该帧的瞬时帧耗时（未平滑），用于区分"瞬时尖峰"与"持续慢帧"。
        struct Sample {
                double time = 0.0;
                float fps = 0.0f;
                float dt_ms = 0.0f;
        };

        std::deque<Sample> samples_;
        // 供 ImGui::PlotLines 使用的扁平数组，每帧从 samples_ 重建。
        std::vector<float> fps_curve_;
        bool pause_ = false;
        bool slot_applied_ = false;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_FPS_DEBUGGER_HPP
