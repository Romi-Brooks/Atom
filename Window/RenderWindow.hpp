/**
  * @file           : RenderWindow.hpp
  * @author         : Romi Brooks
  * @brief          : Main render window singleton (Engine Core)
  * @attention      : Wraps an atom::window::IRenderWindow (created via atom::backend::RenderBackendRegistry)
  *                   behind a stable singleton API. Never depends on a concrete
  *                   backend type; pick one with the backendId argument.
  *                   Overlay/event hooks are multi-slot listeners registered
  *                   with RAII ListenerConnection (ARCH-112).
  * @date           : 2025/9/28
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_RENDER_WINDOW_HPP
#define ATOM_RENDER_WINDOW_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Backend/Contracts/Render/IRenderWindow.hpp>
#include <Backend/Registry/RenderBackendRegistry.hpp>
#include <Window/Manager/ScreenManager.hpp>

namespace atom {

// RAII connection for RenderWindow listener registrations. The listener is
// removed on destruction or explicit Reset(); move-only. Connections remain
// valid for the lifetime of the RenderWindow singleton.
class ListenerConnection {
    public:
        ListenerConnection() = default;
        ~ListenerConnection() {
            Reset();
        }

        ListenerConnection(ListenerConnection&& other) noexcept : remove_(std::move(other.remove_)) {}
        auto operator=(ListenerConnection&& other) noexcept -> ListenerConnection& {
            if (this != &other) {
                Reset();
                remove_ = std::move(other.remove_);
            }
            return *this;
        }
        ListenerConnection(const ListenerConnection&) = delete;
        auto operator=(const ListenerConnection&) -> ListenerConnection& = delete;

        auto Reset() noexcept -> void {
            if (remove_) {
                remove_();
                remove_ = nullptr;
            }
        }

    private:
        friend class RenderWindow;
        explicit ListenerConnection(std::function<void()> remove) : remove_(std::move(remove)) {}

        std::function<void()> remove_;
};

class RenderWindow {
    private:
        std::unique_ptr<atom::window::IRenderWindow> window_;
        std::string backend_id_{};
        unsigned int fps_ = 60;
        bool shutdown_notified_ = false;

        // --- Listener registry storage (multi-slot) ---
        struct RawEventListenerEntry {
                uint64_t id;
                std::function<void(const void*)> fn;
                using ListenerFn = std::function<void(const void*)>;
        };
        struct EventListenerEntry {
                uint64_t id;
                std::function<void(atom::window::IEvent&)> fn;
                using ListenerFn = std::function<void(atom::window::IEvent&)>;
        };
        struct UpdateListenerEntry {
                uint64_t id;
                std::function<void(float)> fn;
                using ListenerFn = std::function<void(float)>;
        };
        struct OverlayListenerEntry {
                uint64_t id;
                std::function<void()> fn;
                using ListenerFn = std::function<void()>;
        };
        struct ResizeListenerEntry {
                uint64_t id;
                std::function<void(uint32_t, uint32_t)> fn;
                using ListenerFn = std::function<void(uint32_t, uint32_t)>;
        };
        struct ShutdownListenerEntry {
                uint64_t id;
                std::function<void()> fn;
                using ListenerFn = std::function<void()>;
        };

        std::vector<RawEventListenerEntry> raw_event_listeners_;
        std::vector<EventListenerEntry> event_listeners_;
        std::vector<UpdateListenerEntry> update_listeners_;
        std::vector<OverlayListenerEntry> overlay_listeners_;
        std::vector<ResizeListenerEntry> resize_listeners_;
        std::vector<ShutdownListenerEntry> shutdown_listeners_;
        uint64_t next_listener_id_ = 1;

        template <typename Entry>
        auto AddListener(std::vector<Entry>& list, typename Entry::ListenerFn listener) -> ListenerConnection {
            const uint64_t id = next_listener_id_++;
            list.push_back(Entry{id, std::move(listener)});
            return ListenerConnection(
                [this, &list, id] { std::erase_if(list, [id](const Entry& e) { return e.id == id; }); });
        }

        auto ProcessEvents(const ScreenManager& screenManager) -> void;

        RenderWindow() = default;
        ~RenderWindow() = default;

    public:
        RenderWindow(const RenderWindow&) = delete;
        auto operator=(const RenderWindow&) -> RenderWindow& = delete;

        [[nodiscard]] static auto GetInstance() -> RenderWindow&;

        // --- Listener registry (multiple listeners may coexist; unregistering one
        // never clears the others). Listeners must not unregister themselves while
        // being dispatched. ---

        using RawEventListener =
            std::function<void(const void*)>; // raw backend event, before translation (platform adapters only)
        using EventListener = std::function<void(atom::window::IEvent&)>; // translated engine event
        using UpdateListener = std::function<void(float)>;                // per frame, before rendering
        using OverlayListener = std::function<void()>;                    // per frame, after scene render
        using ResizeListener = std::function<void(uint32_t, uint32_t)>;   // after backend HandleResize
        using ShutdownListener = std::function<void()>;                   // once, on Shutdown

        [[nodiscard]] auto AddRawEventListener(RawEventListener listener) -> ListenerConnection;
        [[nodiscard]] auto AddEventListener(EventListener listener) -> ListenerConnection;
        [[nodiscard]] auto AddUpdateListener(UpdateListener listener) -> ListenerConnection;
        [[nodiscard]] auto AddOverlayListener(OverlayListener listener) -> ListenerConnection;
        [[nodiscard]] auto AddResizeListener(ResizeListener listener) -> ListenerConnection;
        [[nodiscard]] auto AddShutdownListener(ShutdownListener listener) -> ListenerConnection;

        // Core API
        // backendId selects the render backend (e.g. "sdl3", or a custom backend
        // registered in atom::backend::RenderBackendRegistry). Defaults to the engine default.
        auto Initialize(const std::string& title, algo::Vec2 resolution,
                        std::string_view backendId = atom::backend::RenderBackendRegistry::kDefaultBackendId) -> void;
        auto Run() -> void;
        auto SetFPS(unsigned int fps) -> void;
        [[nodiscard]] auto GetFPS() const -> unsigned;
        auto SetVSync(bool enabled) -> bool;
        [[nodiscard]] auto IsVSyncEnabled() const -> bool;
        [[nodiscard]] auto IsOpen() const -> bool;
        auto Shutdown() -> void;

        // Backend access
        [[nodiscard]] auto GetIRenderWindow() -> atom::window::IRenderWindow*;
        [[nodiscard]] auto GetBackendId() const -> const std::string&;
};

} // namespace atom

#endif // ATOM_RENDER_WINDOW_HPP
