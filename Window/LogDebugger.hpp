/**
  * @file           : LogDebugger.hpp
  * @brief          : ImGui log viewer component.
  */

#ifndef ATOM_LOG_DEBUGGER_HPP
#define ATOM_LOG_DEBUGGER_HPP

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include <Log/LogSystem.hpp>

namespace atom {

namespace debugger {
class OverlayConnection;
}
class RenderWindow;

// Optional logger panel used by Debugger and also usable as a standalone
// component. It has no effect on the lifetime or implementation of Atom_Log.
class LogDebugger final {
    private:
        struct State {
                std::mutex mutex;
                std::deque<LogRecord> records;
        };

    public:
        LogDebugger() = default;
        ~LogDebugger();

        auto Attach(RenderWindow& window) -> void;
        auto Detach() -> void;
        auto SetEnabled(bool enabled) -> void;

        [[nodiscard]] auto IsAttached() const -> bool {
            return attached_;
        }
        [[nodiscard]] auto IsEnabled() const -> bool {
            return enabled_;
        }

    private:
        auto OnDrawOverlay() -> void;

        bool attached_ = false;
        bool enabled_ = true;
        RenderWindow* target_window_ = nullptr;
        std::unique_ptr<debugger::OverlayConnection> overlay_connection_;
        std::shared_ptr<State> state_;
        LogConnection log_connection_;
        LogLevel minimum_level_ = LogLevel::ATOM_DEBUG;
        std::unordered_set<std::string> selected_channels_;
        bool auto_scroll_ = true;
        bool window_open_ = true;
        char text_filter_[256]{};
        std::size_t max_entries_ = 10000;
};

} // namespace atom

#endif // ATOM_LOG_DEBUGGER_HPP
