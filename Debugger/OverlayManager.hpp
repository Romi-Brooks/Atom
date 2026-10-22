/**
  * @file           : OverlayManager.hpp
  * @brief          : Runtime-integrated shared ImGui overlay lifetime.
  */

#ifndef ATOM_OVERLAY_MANAGER_HPP
#define ATOM_OVERLAY_MANAGER_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace atom {

class RenderWindow;
namespace runtime {
class RuntimeConnection;
}

namespace debugger {

class IDebugImGuiBackend;

// RAII connection for one overlay panel. Resetting a panel never affects any
// other panel or the shared ImGui backend.
class OverlayConnection {
    public:
        OverlayConnection() = default;
        ~OverlayConnection() {
            Reset();
        }

        OverlayConnection(OverlayConnection&& other) noexcept : remove_(std::move(other.remove_)) {}
        auto operator=(OverlayConnection&& other) noexcept -> OverlayConnection& {
            if (this != &other) {
                Reset();
                remove_ = std::move(other.remove_);
            }
            return *this;
        }
        OverlayConnection(const OverlayConnection&) = delete;
        auto operator=(const OverlayConnection&) -> OverlayConnection& = delete;

        auto Reset() noexcept -> void {
            if (remove_) {
                remove_();
                remove_ = nullptr;
            }
        }

        [[nodiscard]] auto IsConnected() const noexcept -> bool {
            return static_cast<bool>(remove_);
        }

    private:
        friend class OverlayManager;
        explicit OverlayConnection(std::function<void()> remove) : remove_(std::move(remove)) {}

        std::function<void()> remove_;
};

// Owns the single ImGui frame/backend lifecycle for one RenderWindow and
// dispatches all registered panels inside that shared context.
class OverlayManager final {
    public:
        using DrawCallback = std::function<void()>;

        ~OverlayManager();

        OverlayManager(const OverlayManager&) = delete;
        auto operator=(const OverlayManager&) -> OverlayManager& = delete;

        [[nodiscard]] auto AddPanel(DrawCallback callback) -> OverlayConnection;
        [[nodiscard]] static auto GetInstance(RenderWindow& window) -> OverlayManager&;
        [[nodiscard]] auto IsInitialized() const noexcept -> bool {
            return initialized_;
        }

    private:
        struct PanelEntry {
                std::size_t id;
                DrawCallback callback;
        };

        auto EnsureInitialized() -> bool;
        auto ShutdownBackend() -> void;

        explicit OverlayManager(RenderWindow& window);

        RenderWindow& window_;
        std::unique_ptr<IDebugImGuiBackend> backend_;
        std::unique_ptr<runtime::RuntimeConnection> event_connection_;
        std::unique_ptr<runtime::RuntimeConnection> update_connection_;
        std::unique_ptr<runtime::RuntimeConnection> overlay_connection_;
        std::unique_ptr<runtime::RuntimeConnection> shutdown_connection_;
        std::vector<PanelEntry> panels_;
        std::size_t next_panel_id_ = 1;
        bool initialized_ = false;
};

} // namespace debugger
} // namespace atom

#endif // ATOM_OVERLAY_MANAGER_HPP
