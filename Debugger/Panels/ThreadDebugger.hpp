// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file ThreadDebugger.hpp
 * @brief ImGui diagnostic panel listing the engine's live threads.
 *
 * Shows which threads are registered (job workers, the log console writer,
 * audio decode threads, the loader, and the render/main thread) and, per
 * thread, the cumulative CPU time plus the CPU consumed over the last sample
 * window. Threads are sorted by that recent CPU so the "heaviest" thread — the
 * usual suspect for a frame-time regression — floats to the top.
 */

#ifndef ATOM_DEBUGGER_THREAD_DEBUGGER_HPP
#define ATOM_DEBUGGER_THREAD_DEBUGGER_HPP

#include <thread>
#include <unordered_map>
#include <vector>

#include <Debugger/DebugPanel.hpp>

namespace atom::debugger {

class ThreadDebugger final : public DebugPanel {
    public:
        ThreadDebugger() : DebugPanel("ThreadDebugger") {}
        ~ThreadDebugger() override;

    protected:
        auto OnDrawOverlay() -> void override;

    private:
        struct Row {
                std::string name;
                bool alive = false;
                double total_ms = 0.0;
                double delta_ms = 0.0;
        };

        auto EnsureMainHandle() -> void;

        void* main_handle_ = nullptr;                 // duplicated render-thread handle
        std::unordered_map<std::thread::id, double> prev_cpu_{};
        double last_sample_ = 0.0;
        std::vector<Row> display_{};
        bool pause_ = false;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_THREAD_DEBUGGER_HPP
