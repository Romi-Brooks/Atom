#include "OverlayManager.hpp"

#include <algorithm>
#include <Backend/Contracts/Debug/IDebugImGuiBackend.hpp>
#include <Backend/Registry/DebugImGuiBackendRegistry.hpp>
#include <Log/LogSystem.hpp>
#include <Window/RenderWindow.hpp>

namespace atom::debugger {

OverlayManager::OverlayManager(RenderWindow& window) : window_(window) {}

OverlayManager::~OverlayManager() {
    raw_event_connection_.reset();
    update_connection_.reset();
    overlay_connection_.reset();
    shutdown_connection_.reset();
    ShutdownBackend();
}

auto OverlayManager::EnsureInitialized() -> bool {
    if (initialized_)
        return true;

    auto* platform_window = window_.GetIWindow();
    auto* render_device = window_.GetRenderDevice();
    if (!platform_window || !render_device) {
        LOG_WARNING(atom::debugger::LogChannel::IMGUI,
                    "Overlay manager requires an initialized render window and device");
        return false;
    }

    backend_ = DebugImGuiBackendRegistry::GetInstance().Create(window_.GetBackendId(), *platform_window, *render_device);
    if (!backend_ || !backend_->Initialize()) {
        LOG_ERROR(atom::debugger::LogChannel::IMGUI,
                  "Overlay manager initialization failed for render backend '" + window_.GetBackendId() + "'");
        backend_.reset();
        return false;
    }

    raw_event_connection_ = std::make_unique<ListenerConnection>(
        window_.AddRawEventListener([this](const void* raw_event) {
            if (backend_)
                backend_->ProcessRawEvent(raw_event);
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
}

auto OverlayManager::AddPanel(DrawCallback callback) -> OverlayConnection {
    if (!callback || !EnsureInitialized())
        return {};

    const auto id = next_panel_id_++;
    panels_.push_back(PanelEntry{id, std::move(callback)});
    return OverlayConnection([this, id] {
        std::erase_if(panels_, [id](const PanelEntry& panel) { return panel.id == id; });
    });
}

auto OverlayManager::OnRenderWindowInitialized() -> void {
    if (!panels_.empty())
        EnsureInitialized();
}

auto OverlayManager::ShutdownBackend() -> void {
    if (backend_) {
        backend_->Shutdown();
        backend_.reset();
    }
    initialized_ = false;
}

} // namespace atom::debugger
