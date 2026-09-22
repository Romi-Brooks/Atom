#include "OverlayManager.hpp"

#include <algorithm>
#include <string>
#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Extension/DebugImGuiBackendRegistry.hpp>
#include <Debugger/DebuggerConfig.hpp>
#include <Log/LogSystem.hpp>
#include <Window/RenderWindow.hpp>

namespace atom::debugger {

OverlayManager::OverlayManager(RenderWindow& window) : window_(window) {}

OverlayManager::~OverlayManager() {
    event_connection_.reset();
    update_connection_.reset();
    overlay_connection_.reset();
    shutdown_connection_.reset();
    ShutdownBackend();
}

auto OverlayManager::EnsureInitialized() -> bool {
#if !ATOM_ENABLE_DEBUGGER
    return false;
#else
    if (initialized_)
        return true;

    auto* platform_window = window_.GetIWindow();
    auto* render_device = window_.GetRenderDevice();
    if (!platform_window || !render_device) {
        LOG_WARNING(atom::log::debugger::ImGui,
                    "Overlay manager requires an initialized render window and device");
        return false;
    }

    backend_ = DebugImGuiBackendRegistry::GetInstance().Create(window_.GetBackendId(), *platform_window, *render_device);
    if (!backend_ || !backend_->Initialize()) {
        LOG_ERROR(atom::log::debugger::ImGui,
                  "Overlay manager initialization failed for render backend '" + window_.GetBackendId() + "'");
        backend_.reset();
        return false;
    }

    event_connection_ = std::make_unique<ListenerConnection>(
        window_.AddEventListener([this](window::IEvent& event) {
            if (backend_)
                backend_->ProcessEvent(event);
        }));
    update_connection_ = std::make_unique<ListenerConnection>(window_.AddUpdateListener([this](float) {
        if (backend_)
            backend_->NewFrame();
    }));
    overlay_connection_ = std::make_unique<ListenerConnection>(window_.AddOverlayListener([this] {
        for (const auto& panel : panels_) {
            if (panel.callback)
                panel.callback();
        }
        if (backend_)
            backend_->Render();
    }));
    shutdown_connection_ = std::make_unique<ListenerConnection>(window_.AddShutdownListener([this] {
        ShutdownBackend();
    }));

    initialized_ = true;
    return true;
#endif
}

auto OverlayManager::AddPanel(DrawCallback callback) -> OverlayConnection {
#if !ATOM_ENABLE_DEBUGGER
    (void)callback;
    return {};
#else
    if (!callback || !EnsureInitialized())
        return {};

    const auto id = next_panel_id_++;
    panels_.push_back(PanelEntry{id, std::move(callback)});
    return OverlayConnection([this, id] {
        std::erase_if(panels_, [id](const PanelEntry& panel) { return panel.id == id; });
    });
#endif
}

auto OverlayManager::OnRenderWindowInitialized() -> void {
#if ATOM_ENABLE_DEBUGGER
    if (!panels_.empty())
        EnsureInitialized();
#endif
}

auto OverlayManager::ShutdownBackend() -> void {
    if (!panels_.empty()) {
        LOG_WARNING(atom::log::debugger::ImGui,
                    "Overlay shutting down with " + std::to_string(panels_.size()) +
                        " debug panel(s) still attached");
    }
    if (backend_) {
        backend_->Shutdown();
        backend_.reset();
    }
    initialized_ = false;
}

} // namespace atom::debugger
