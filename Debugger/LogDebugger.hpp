// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file LogDebugger.hpp
 * @brief ImGui log viewer panel (is-a DebugPanel). Well-known single instance.
 */

#ifndef ATOM_DEBUGGER_LOG_DEBUGGER_HPP
#define ATOM_DEBUGGER_LOG_DEBUGGER_HPP

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include <Debugger/DebugPanel.hpp>
#include <Log/LogSystem.hpp>

namespace atom::debugger {

// Visibility (SetEnabled) is independent of lifetime (Attach/Detach):
// closing the panel with X only hides it; Detach unsubscribes and drops the buffer.
//
// There is at most one live LogDebugger. Other panels toggle it via Get()
// instead of holding a constructor-injected pointer. A second Attach is
// rejected with a warning.
class LogDebugger final : public DebugPanel {
    private:
        struct State {
                std::mutex mutex;
                std::deque<LogRecord> records;
        };

    public:
        LogDebugger() : DebugPanel("LogDebugger") {}
        ~LogDebugger() override;

        // The attached log panel, or nullptr when none is attached /
        // ATOM_ENABLE_DEBUGGER is 0.
        [[nodiscard]] static auto Get() -> LogDebugger*;

        auto SetEnabled(bool enabled) -> void override;

    protected:
        auto OnAttach(atom::RenderWindow& window) -> bool override;
        auto OnDetach() -> void override;
        auto OnDrawOverlay() -> void override;

    private:
        std::shared_ptr<State> state_;
        LogConnection log_connection_;
        LogLevel minimum_level_ = LogLevel::ATOM_DEBUG;
        std::unordered_set<std::string> selected_channels_;
        bool auto_scroll_ = true;
        bool window_open_ = true;
        bool slot_applied_ = false;
        char text_filter_[256]{};
        std::size_t max_entries_ = 10000;
};

} // namespace atom::debugger

#endif // ATOM_DEBUGGER_LOG_DEBUGGER_HPP
