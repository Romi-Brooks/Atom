/**
 * @file           : DisplaySystemTest.cpp
 * @brief          : The display policy's contract: queue, apply once, report
 *                   the truth.
 *
 * This logic is small but has three failure modes that are invisible until a
 * player hits them: rebuilding the swapchain every frame (a settings page
 * re-requests the current value on every redraw), retrying a rejected mode
 * forever, and claiming a change was applied when the backend refused it.
 * Each one is asserted below against recording doubles, so no window or GPU is
 * needed.
 */

#include <Display/DisplaySystem.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

// Records what the display system asks for, and can refuse on demand.
class RecordingWindow final : public atom::window::IWindow {
    public:
        auto Initialize(const std::string&, atom::algo::Vec2) -> bool override { return true; }
        auto Shutdown() -> void override {}
        auto RequestClose() -> void override {}
        [[nodiscard]] auto IsOpen() const -> bool override { return true; }
        [[nodiscard]] auto PollEvent() -> std::optional<atom::window::IEvent> override { return std::nullopt; }
        auto SetFPS(uint32_t fps) -> void override { fps_ = fps; }
        [[nodiscard]] auto GetFPS() const -> uint32_t override { return fps_; }
        [[nodiscard]] auto GetTimeSeconds() const -> double override { return 0.0; }
        auto WaitForNextFrame() -> void override {}
        [[nodiscard]] auto GetSize() const -> atom::algo::Vec2 override { return {800.0f, 600.0f}; }

        auto SetWindowMode(atom::window::WindowMode mode) -> bool override {
            ++set_mode_calls;
            if (reject_window_mode)
                return false;
            mode_ = mode;
            return true;
        }
        [[nodiscard]] auto GetWindowMode() const -> atom::window::WindowMode override { return mode_; }

        bool reject_window_mode = false;
        int set_mode_calls = 0;

    private:
        atom::window::WindowMode mode_ = atom::window::WindowMode::Windowed;
        uint32_t fps_ = 60;
};

class RecordingDevice final : public atom::render::IRenderDevice {
    public:
        [[nodiscard]] auto BeginFrame() -> bool override { return true; }
        auto Clear(const atom::render::Color&) -> void override {}
        auto EndFrame() -> void override {}
        auto HandleResize(uint32_t, uint32_t) -> void override {}

        auto SetPresentMode(atom::render::PresentMode mode) -> bool override {
            ++set_present_calls;
            if (reject_present_mode)
                return false;
            present_ = mode;
            return true;
        }
        [[nodiscard]] auto GetPresentMode() const -> atom::render::PresentMode override { return present_; }
        [[nodiscard]] auto SupportedPresentModes() const -> std::uint8_t override {
            return atom::render::all_present_modes;
        }
        [[nodiscard]] auto GetOutputSize() const -> atom::algo::Vec2 override { return {800.0f, 600.0f}; }
        [[nodiscard]] auto GetBackendInfo() const -> const atom::render::RenderBackendInfo& override { return info_; }

        bool reject_present_mode = false;
        int set_present_calls = 0;

    private:
        atom::render::PresentMode present_ = atom::render::PresentMode::VSync;
        atom::render::RenderBackendInfo info_{"null", "null", "", "", 0, 0};
};

} // namespace

auto main() -> int {
    auto& display = atom::display::DisplaySystem::GetInstance();
    RecordingWindow window{};
    RecordingDevice device{};

    // --- startup ----------------------------------------------------------
    display.Adopt({atom::window::WindowMode::Windowed, atom::render::PresentMode::VSync});
    ATOM_CHECK(display.Applied().window_mode == atom::window::WindowMode::Windowed);
    ATOM_CHECK(display.Applied().present_mode == atom::render::PresentMode::VSync);
    ATOM_CHECK(!display.HasPendingChange());

    // Adopting again must not queue anything either.
    display.ApplyPending(window, device);
    ATOM_CHECK(window.set_mode_calls == 0);
    ATOM_CHECK(device.set_present_calls == 0);

    // --- re-requesting the applied value is a no-op -----------------------
    // A settings page redraws its combo boxes every frame, so this is the hot
    // path: queueing here would rebuild the swapchain at frame rate.
    display.Request({atom::window::WindowMode::Windowed, atom::render::PresentMode::VSync});
    ATOM_CHECK(!display.HasPendingChange());
    display.ApplyPending(window, device);
    ATOM_CHECK(window.set_mode_calls == 0);
    ATOM_CHECK(device.set_present_calls == 0);

    // --- a real change is queued, then applied exactly once ---------------
    display.Request({atom::window::WindowMode::Borderless, atom::render::PresentMode::Immediate});
    ATOM_CHECK(display.HasPendingChange());
    ATOM_CHECK(display.Requested().present_mode == atom::render::PresentMode::Immediate);
    // Requesting does not touch the backend: it cannot, mid-frame.
    ATOM_CHECK(window.set_mode_calls == 0);
    ATOM_CHECK(device.set_present_calls == 0);

    display.ApplyPending(window, device);
    ATOM_CHECK(!display.HasPendingChange());
    ATOM_CHECK(window.set_mode_calls == 1);
    ATOM_CHECK(device.set_present_calls == 1);
    ATOM_CHECK(display.Applied().window_mode == atom::window::WindowMode::Borderless);
    ATOM_CHECK(display.Applied().present_mode == atom::render::PresentMode::Immediate);

    // A second apply with nothing queued must not repeat the work.
    display.ApplyPending(window, device);
    ATOM_CHECK(window.set_mode_calls == 1);
    ATOM_CHECK(device.set_present_calls == 1);

    // --- one axis at a time ----------------------------------------------
    display.RequestWindowMode(atom::window::WindowMode::Windowed);
    display.ApplyPending(window, device);
    ATOM_CHECK(window.set_mode_calls == 2);
    ATOM_CHECK(device.set_present_calls == 1); // present mode untouched

    display.RequestPresentMode(atom::render::PresentMode::VSync);
    display.ApplyPending(window, device);
    ATOM_CHECK(window.set_mode_calls == 2);
    ATOM_CHECK(device.set_present_calls == 2);

    // --- a rejected change must not be retried forever --------------------
    device.reject_present_mode = true;
    const auto before_reject = device.set_present_calls;
    display.RequestPresentMode(atom::render::PresentMode::Mailbox);
    display.ApplyPending(window, device);
    ATOM_CHECK(device.set_present_calls == before_reject + 1);
    ATOM_CHECK(!display.HasPendingChange());
    // Applied still reports reality, not the rejected request.
    ATOM_CHECK(display.Applied().present_mode == atom::render::PresentMode::VSync);

    // The next frame must not try again on its own.
    display.ApplyPending(window, device);
    ATOM_CHECK(device.set_present_calls == before_reject + 1);
    device.reject_present_mode = false;

    // --- the same for the window placement -------------------------------
    window.reject_window_mode = true;
    const auto before_window_reject = window.set_mode_calls;
    display.RequestWindowMode(atom::window::WindowMode::ExclusiveFullscreen);
    display.ApplyPending(window, device);
    ATOM_CHECK(window.set_mode_calls == before_window_reject + 1);
    ATOM_CHECK(display.Applied().window_mode == atom::window::WindowMode::Windowed);
    display.ApplyPending(window, device);
    ATOM_CHECK(window.set_mode_calls == before_window_reject + 1);
    window.reject_window_mode = false;

    // --- RequestVSync keeps the old two-state meaning --------------------
    display.RequestVSync(false);
    display.ApplyPending(window, device);
    ATOM_CHECK(display.Applied().present_mode == atom::render::PresentMode::Immediate);
    display.RequestVSync(true);
    display.ApplyPending(window, device);
    ATOM_CHECK(display.Applied().present_mode == atom::render::PresentMode::VSync);

    // --- ToString identifies a measurement --------------------------------
    ATOM_CHECK(atom::display::ToString({atom::window::WindowMode::Borderless, atom::render::PresentMode::Immediate}) ==
               "borderless+immediate");

    return 0;
}
