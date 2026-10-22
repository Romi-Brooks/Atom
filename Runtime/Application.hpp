/**
 * @file Application.hpp
 * @brief Engine runtime composition and the sole owner of the game frame loop.
 */
#ifndef ATOM_RUNTIME_APPLICATION_HPP
#define ATOM_RUNTIME_APPLICATION_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Render/RenderBackendId.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Error/Error.hpp>

namespace atom {
class RenderWindow;
class ScreenManager;
namespace render {
class IRenderDevice;
}
}

namespace atom::runtime {

// Move-only subscription for non-game observers such as debugger overlays and
// diagnostics. Game systems are scheduled explicitly by Application; they do
// not register anonymous callbacks into the window layer.
class RuntimeConnection final {
    public:
        RuntimeConnection() = default;
        ~RuntimeConnection() { Reset(); }
        RuntimeConnection(RuntimeConnection&& other) noexcept : remove_(std::move(other.remove_)) {}
        auto operator=(RuntimeConnection&& other) noexcept -> RuntimeConnection& {
            if (this != &other) {
                Reset();
                remove_ = std::move(other.remove_);
            }
            return *this;
        }
        RuntimeConnection(const RuntimeConnection&) = delete;
        auto operator=(const RuntimeConnection&) -> RuntimeConnection& = delete;

        auto Reset() noexcept -> void {
            if (remove_) {
                remove_();
                remove_ = nullptr;
            }
        }

    private:
        friend class Application;
        explicit RuntimeConnection(std::function<void()> remove) : remove_(std::move(remove)) {}
        std::function<void()> remove_;
};

struct ApplicationConfig final {
        std::string title = "Atom";
        algo::Vec2 resolution{1280.0f, 720.0f};
        backend::RenderBackendId render_backend = backend::RenderBackendId::SdlGpu;
};

class Application final {
    public:
        using EventObserver = std::function<void(window::IEvent&)>;
        using UpdateObserver = std::function<void(float)>;
        using FixedUpdateObserver = std::function<void(float)>;
        using OverlayObserver = std::function<void()>;
        using ResizeObserver = std::function<void(uint32_t, uint32_t)>;
        using ShutdownObserver = std::function<void()>;

        [[nodiscard]] static auto GetInstance() -> Application&;

        Application(const Application&) = delete;
        auto operator=(const Application&) -> Application& = delete;

        [[nodiscard]] auto Initialize(const ApplicationConfig& config = {}) -> Result<void>;
        [[nodiscard]] auto Initialize(std::string title, algo::Vec2 resolution,
                                      backend::RenderBackendId render_backend = backend::RenderBackendId::SdlGpu)
            -> Result<void>;
        auto Run() -> void;
        auto Shutdown() -> void;
        auto RequestStop() -> void;
        [[nodiscard]] auto IsInitialized() const noexcept -> bool { return initialized_; }
        [[nodiscard]] auto IsRunning() const noexcept -> bool { return running_; }

        [[nodiscard]] auto GetWindow() -> RenderWindow&;
        [[nodiscard]] auto GetScreenManager() -> ScreenManager&;

        // Observers are intentionally limited to tooling/host integration.
        // Core game and screen dispatch is explicit in Run(). An observer must
        // not reset its own connection while its callback is being dispatched.
        [[nodiscard]] auto AddEventObserver(EventObserver observer) -> RuntimeConnection;
        [[nodiscard]] auto AddUpdateObserver(UpdateObserver observer) -> RuntimeConnection;
        [[nodiscard]] auto AddFixedUpdateObserver(FixedUpdateObserver observer) -> RuntimeConnection;
        [[nodiscard]] auto AddOverlayObserver(OverlayObserver observer) -> RuntimeConnection;
        [[nodiscard]] auto AddResizeObserver(ResizeObserver observer) -> RuntimeConnection;
        [[nodiscard]] auto AddShutdownObserver(ShutdownObserver observer) -> RuntimeConnection;

    private:
        template <typename Callback> struct ObserverEntry final {
                uint64_t id = 0;
                Callback callback;
        };
        using EventEntry = ObserverEntry<EventObserver>;
        using UpdateEntry = ObserverEntry<UpdateObserver>;
        using FixedEntry = ObserverEntry<FixedUpdateObserver>;
        using OverlayEntry = ObserverEntry<OverlayObserver>;
        using ResizeEntry = ObserverEntry<ResizeObserver>;
        using ShutdownEntry = ObserverEntry<ShutdownObserver>;

        Application() = default;
        template <typename Entry, typename Callback>
        auto AddObserver(std::vector<Entry>& entries, Callback observer) -> RuntimeConnection;
        auto DispatchEvent(window::IEvent& event) -> void;

        std::vector<EventEntry> event_observers_{};
        std::vector<UpdateEntry> update_observers_{};
        std::vector<FixedEntry> fixed_update_observers_{};
        std::vector<OverlayEntry> overlay_observers_{};
        std::vector<ResizeEntry> resize_observers_{};
        std::vector<ShutdownEntry> shutdown_observers_{};
        uint64_t next_observer_id_ = 1;
        bool initialized_ = false;
        bool running_ = false;
        bool shutdown_notified_ = false;
};

} // namespace atom::runtime

#endif // ATOM_RUNTIME_APPLICATION_HPP
