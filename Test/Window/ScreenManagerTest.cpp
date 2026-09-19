#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Test/Support/TestHelpers.hpp>
#include <Window/Manager/ScreenManager.hpp>
#include <Window/Screen.hpp>

#include <memory>
#include <string>

namespace {

class NullRenderDevice final : public atom::render::IRenderDevice {
    public:
        auto BeginFrame() -> bool override { return true; }
        auto Clear(const atom::render::Color&) -> void override {}
        auto EndFrame() -> void override {}
        auto HandleResize(uint32_t, uint32_t) -> void override {}
        auto SetVSync(bool) -> bool override { return true; }
        [[nodiscard]] auto IsVSyncEnabled() const -> bool override { return false; }
        [[nodiscard]] auto GetOutputSize() const -> atom::algo::Vec2 override { return {800.0f, 600.0f}; }
        [[nodiscard]] auto GetBackendInfo() const -> const atom::render::RenderBackendInfo& override { return info_; }

    private:
        atom::render::RenderBackendInfo info_{"null", "null", "null", 0, 0};
};

class CountingScreen final : public atom::Screen {
    public:
        explicit CountingScreen(std::string name) : name_(std::move(name)) {}

        auto Render(atom::render::IRenderDevice&) -> void override { ++renders; }
        auto HandleEvent(const atom::window::IEvent&) -> bool override {
            ++events;
            return consume_events;
        }
        auto Update(float) -> void override { ++updates; }
        auto OnActivate() -> void override { ++activates; }
        auto OnDeactivate() -> void override { ++deactivates; }

        std::string name_{};
        int renders = 0;
        int events = 0;
        int updates = 0;
        int activates = 0;
        int deactivates = 0;
        bool consume_events = false;

    private:
};

} // namespace

auto main() -> int {
    auto& manager = atom::ScreenManager::GetInstance();

    auto home = std::make_unique<CountingScreen>("home");
    auto* home_ptr = home.get();
    auto settings = std::make_unique<CountingScreen>("settings");
    auto* settings_ptr = settings.get();
    auto pause = std::make_unique<CountingScreen>("pause");
    auto* pause_ptr = pause.get();

    manager.LoadScreen("home", std::move(home));
    manager.LoadScreen("settings", std::move(settings));
    manager.LoadScreen("pause", std::move(pause));

    ATOM_CHECK(manager.GetCurrentScreenName() == "");

    manager.SwitchScreen("home");
    ATOM_CHECK(manager.GetCurrentScreenName() == "home");
    ATOM_CHECK(home_ptr->activates == 1);
    ATOM_CHECK(manager.GetScreenStack().empty());

    manager.SwitchScreen("settings");
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");
    ATOM_CHECK(home_ptr->deactivates == 1);
    ATOM_CHECK(settings_ptr->activates == 1);

    manager.PushScreen("pause");
    ATOM_CHECK(manager.GetCurrentScreenName() == "pause");
    ATOM_CHECK(settings_ptr->deactivates == 1);
    ATOM_CHECK(pause_ptr->activates == 1);
    ATOM_CHECK(manager.GetScreenStack().size() == 1);
    ATOM_CHECK(manager.GetScreenStack().back().first == "settings");

    manager.PushScreen("home");
    ATOM_CHECK(manager.GetCurrentScreenName() == "home");
    ATOM_CHECK(manager.GetScreenStack().size() == 2);
    ATOM_CHECK(home_ptr->activates == 2);

    manager.Update(0.016f);
    ATOM_CHECK(home_ptr->updates == 1);
    ATOM_CHECK(settings_ptr->updates == 0);
    ATOM_CHECK(pause_ptr->updates == 0);

    NullRenderDevice device{};
    manager.Render(device);
    ATOM_CHECK(home_ptr->renders == 1);
    ATOM_CHECK(pause_ptr->renders == 1);
    ATOM_CHECK(settings_ptr->renders == 1);

    atom::window::IEvent event{};
    event.type = atom::window::EventType::KeyPressed;
    home_ptr->consume_events = true;
    manager.HandleEvent(event);
    ATOM_CHECK(home_ptr->events == 1);
    ATOM_CHECK(pause_ptr->events == 0);
    home_ptr->consume_events = false;
    manager.HandleEvent(event);
    ATOM_CHECK(home_ptr->events == 2);
    ATOM_CHECK(pause_ptr->events == 1);

    manager.PopScreen();
    ATOM_CHECK(manager.GetCurrentScreenName() == "pause");
    ATOM_CHECK(manager.GetScreenStack().size() == 1);
    manager.PopScreen();
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");
    ATOM_CHECK(manager.GetScreenStack().empty());
    manager.PopScreen(); // empty stack is a no-op
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");

    manager.SwitchScreen("missing-screen");
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");
    manager.PushScreen("missing-screen");
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");
    return 0;
}
