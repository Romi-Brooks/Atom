#include <Backend/Contracts/Render/IRenderDevice.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Log/LogSystem.hpp>
#include <Test/Support/TestHelpers.hpp>
#include <Window/Screen.hpp>
#include <Window/ScreenManager.hpp>

#include <memory>
#include <functional>
#include <string>
#include <utility>

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

// Per-name tallies that outlive every screen generation, so the replacement
// assertions can compare deltas without touching a destroyed object.
struct ScreenCounters {
        int destroys = 0;
        int activates = 0;
        int deactivates = 0;
};

// CountingScreen records lifecycle calls on itself (readable while it is alive)
// and, optionally, into an external ScreenCounters (readable after it is gone).
// `on_deactivate` lets a test call back into the ScreenManager from inside a
// lifecycle callback, which is the re-entrancy case the manager must survive.
class CountingScreen final : public atom::Screen {
    public:
        explicit CountingScreen(std::string name, ScreenCounters* counters = nullptr)
            : name_(std::move(name)), counters_(counters) {}

        ~CountingScreen() override {
            if (counters_)
                ++counters_->destroys;
        }

        auto Render(atom::render::IRenderDevice&) -> void override { ++renders; }
        auto HandleEvent(const atom::window::IEvent&) -> bool override {
            ++events;
            return consume_events;
        }
        auto Update(float) -> void override { ++updates; }
        auto OnActivate() -> void override {
            ++activates;
            if (counters_)
                ++counters_->activates;
        }
        auto OnDeactivate() -> void override {
            ++deactivates;
            if (counters_)
                ++counters_->deactivates;
            if (on_deactivate)
                on_deactivate();
        }

        std::string name_{};
        int renders = 0;
        int events = 0;
        int updates = 0;
        int activates = 0;
        int deactivates = 0;
        bool consume_events = false;
        std::function<void()> on_deactivate{};

    private:
        ScreenCounters* counters_ = nullptr;
};

} // namespace

auto main() -> int {
    // The re-entrancy case registers and unregisters 64 screens per callback;
    // keep the test output readable.
    atom::Log::SetConsoleSinkEnabled(false);

    auto& manager = atom::ScreenManager::GetInstance();

    // --- registration results (ARCH-105) ------------------------------------
    ATOM_CHECK(manager.LoadScreen("home", nullptr) == atom::ScreenOpResult::NullScreen);
    ATOM_CHECK(manager.LoadScreen("", std::make_unique<CountingScreen>("unnamed")) ==
               atom::ScreenOpResult::InvalidName);

    ScreenCounters home_counters{};
    ScreenCounters settings_counters{};
    ScreenCounters pause_counters{};

    auto home = std::make_unique<CountingScreen>("home", &home_counters);
    auto* home_ptr = home.get();
    auto settings = std::make_unique<CountingScreen>("settings", &settings_counters);
    auto* settings_ptr = settings.get();
    auto pause = std::make_unique<CountingScreen>("pause", &pause_counters);
    auto* pause_ptr = pause.get();

    ATOM_CHECK(manager.LoadScreen("home", std::move(home)) == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.LoadScreen("settings", std::move(settings)) == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.LoadScreen("pause", std::move(pause)) == atom::ScreenOpResult::Success);

    // A duplicate name is rejected instead of silently destroying the screen
    // current_screen_ / screen_stack_ may still be pointing at.
    ATOM_CHECK(manager.LoadScreen("home", std::make_unique<CountingScreen>("home-2")) ==
               atom::ScreenOpResult::NameConflict);
    ATOM_CHECK(manager.GetScreen("home") == home_ptr);
    ATOM_CHECK(home_counters.destroys == 0);

    ATOM_CHECK(manager.GetScreen("missing") == nullptr);
    ATOM_CHECK(manager.GetScreenName(home_ptr) == "home");
    ATOM_CHECK(manager.GetScreenName(nullptr).empty());

    ATOM_CHECK(manager.GetCurrentScreenName() == "");
    ATOM_CHECK(manager.GetCurrentScreen() == nullptr);

    ATOM_CHECK(manager.SwitchScreen("missing") == atom::ScreenOpResult::NotFound);
    ATOM_CHECK(manager.SwitchScreen(nullptr) == atom::ScreenOpResult::NullScreen);
    CountingScreen never_registered{"never-registered"};
    ATOM_CHECK(manager.SwitchScreen(&never_registered) == atom::ScreenOpResult::Unregistered);
    ATOM_CHECK(manager.PushScreen(&never_registered) == atom::ScreenOpResult::Unregistered);

    // --- switching, stacking, per-frame dispatch ---------------------------
    ATOM_CHECK(manager.SwitchScreen("home") == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.GetCurrentScreenName() == "home");
    ATOM_CHECK(manager.GetCurrentScreen() == home_ptr);
    ATOM_CHECK(home_ptr->activates == 1);
    ATOM_CHECK(manager.GetScreenStack().empty());

    // Re-selecting the already active screen is a no-op, not a re-activation.
    ATOM_CHECK(manager.SwitchScreen("home") == atom::ScreenOpResult::AlreadyVisible);
    ATOM_CHECK(home_ptr->activates == 1);
    ATOM_CHECK(manager.PushScreen("home") == atom::ScreenOpResult::AlreadyVisible);
    ATOM_CHECK(manager.GetScreenStack().empty());

    ATOM_CHECK(manager.SwitchScreen("settings") == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");
    ATOM_CHECK(home_ptr->deactivates == 1);
    ATOM_CHECK(settings_ptr->activates == 1);

    ATOM_CHECK(manager.PushScreen("pause") == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.GetCurrentScreenName() == "pause");
    ATOM_CHECK(settings_ptr->deactivates == 1);
    ATOM_CHECK(pause_ptr->activates == 1);
    ATOM_CHECK(manager.GetScreenStack().size() == 1);
    ATOM_CHECK(manager.GetScreenStack().back().name == "settings");
    ATOM_CHECK(manager.GetScreenStack().back().screen == settings_ptr);

    ATOM_CHECK(manager.PushScreen("home") == atom::ScreenOpResult::Success);
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

    ATOM_CHECK(manager.PopScreen() == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.GetCurrentScreenName() == "pause");
    ATOM_CHECK(manager.GetScreenStack().size() == 1);
    ATOM_CHECK(manager.PopScreen() == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");
    ATOM_CHECK(manager.GetScreenStack().empty());
    ATOM_CHECK(manager.PopScreen() == atom::ScreenOpResult::EmptyStack);
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");

    ATOM_CHECK(manager.SwitchScreen("missing-screen") == atom::ScreenOpResult::NotFound);
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");
    ATOM_CHECK(manager.PushScreen("missing-screen") == atom::ScreenOpResult::NotFound);
    ATOM_CHECK(manager.GetCurrentScreenName() == "settings");

    // IsScreenVisible follows Render's set: current + screen stack.
    ATOM_CHECK(manager.SwitchScreen("home") == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.IsScreenVisible(home_ptr));
    ATOM_CHECK(!manager.IsScreenVisible(settings_ptr));
    ATOM_CHECK(!manager.IsScreenVisible(pause_ptr));
    ATOM_CHECK(!manager.IsScreenVisible(nullptr));

    ATOM_CHECK(manager.PushScreen("pause") == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.IsScreenVisible(pause_ptr));
    ATOM_CHECK(manager.IsScreenVisible(home_ptr));

    ATOM_CHECK(manager.PopScreen() == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.IsScreenVisible(home_ptr));
    ATOM_CHECK(!manager.IsScreenVisible(pause_ptr));

    // --- ARCH-105: replacing the active screen must not dangle -------------
    const int home_activates_before_replace = home_counters.activates;
    const int home_deactivates_before_replace = home_counters.deactivates;
    auto home_generation2 = std::make_unique<CountingScreen>("home", &home_counters);
    auto* home_generation2_ptr = home_generation2.get();
    ATOM_CHECK(manager.LoadScreen("home", std::move(home_generation2), atom::ScreenReplacePolicy::Replace) ==
               atom::ScreenOpResult::Success);
    ATOM_CHECK(home_counters.destroys == 1); // generation 1 was released, not leaked
    ATOM_CHECK(home_counters.deactivates == home_deactivates_before_replace + 1); // balanced
    ATOM_CHECK(home_counters.activates == home_activates_before_replace + 1);
    ATOM_CHECK(manager.GetScreen("home") == home_generation2_ptr);
    ATOM_CHECK(manager.GetCurrentScreen() == home_generation2_ptr);
    ATOM_CHECK(manager.GetCurrentScreenName() == "home");
    ATOM_CHECK(!manager.IsScreenVisible(home_ptr)); // no slot still points at generation 1

    ATOM_CHECK(manager.SwitchScreen("settings") == atom::ScreenOpResult::Success);

    // --- ARCH-105: replacing a stacked screen re-points the stack slot -----
    ATOM_CHECK(manager.SwitchScreen("home") == atom::ScreenOpResult::Success); // current: home generation 2
    ATOM_CHECK(manager.PushScreen("settings") == atom::ScreenOpResult::Success); // stack: [home generation 2]
    ATOM_CHECK(manager.GetScreenStack().size() == 1);
    ATOM_CHECK(manager.GetScreenStack().back().screen == home_generation2_ptr);
    auto home_generation3 = std::make_unique<CountingScreen>("home", &home_counters);
    auto* home_generation3_ptr = home_generation3.get();
    ATOM_CHECK(manager.LoadScreen("home", std::move(home_generation3), atom::ScreenReplacePolicy::Replace) ==
               atom::ScreenOpResult::Success);
    ATOM_CHECK(home_counters.destroys == 2);
    ATOM_CHECK(manager.GetCurrentScreen() == settings_ptr); // current is untouched
    ATOM_CHECK(manager.GetScreenStack().size() == 1);
    ATOM_CHECK(manager.GetScreenStack().back().screen == home_generation3_ptr);
    // Popping restores the replacement, never the destroyed screen, and the
    // restore activates it (a pop that forgot OnActivate would still pass a
    // pointer-identity check).
    const int home_activates_before_pop = home_counters.activates;
    ATOM_CHECK(manager.PopScreen() == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.GetCurrentScreen() == home_generation3_ptr);
    ATOM_CHECK(manager.IsScreenVisible(home_generation3_ptr));
    ATOM_CHECK(home_counters.activates == home_activates_before_pop + 1);

    // --- ARCH-105: unload drops the registration and its stack slots -------
    ATOM_CHECK(manager.SwitchScreen("settings") == atom::ScreenOpResult::Success);

    // Not current and not stacked: the registration (and the object) goes away.
    ATOM_CHECK(manager.UnloadScreen("pause") == atom::ScreenOpResult::Success);
    ATOM_CHECK(pause_counters.destroys == 1);
    ATOM_CHECK(manager.GetScreen("pause") == nullptr);
    ATOM_CHECK(manager.UnloadScreen("pause") == atom::ScreenOpResult::NotFound);

    // Current: refused, so the active screen cannot vanish under the renderer.
    ATOM_CHECK(manager.UnloadScreen("settings") == atom::ScreenOpResult::InUse);
    ATOM_CHECK(manager.GetScreen("settings") == settings_ptr);

    // Stacked only: allowed, and the stack slot is dropped together with it.
    ATOM_CHECK(manager.LoadScreen("temp", std::make_unique<CountingScreen>("temp")) == atom::ScreenOpResult::Success);
    ATOM_CHECK(manager.PushScreen("temp") == atom::ScreenOpResult::Success); // stack: [settings], current: temp
    ATOM_CHECK(manager.UnloadScreen("settings") == atom::ScreenOpResult::Success);
    ATOM_CHECK(settings_counters.destroys == 1);
    ATOM_CHECK(manager.GetScreen("settings") == nullptr);
    ATOM_CHECK(manager.GetScreenStack().empty());
    ATOM_CHECK(manager.PopScreen() == atom::ScreenOpResult::EmptyStack);
    ATOM_CHECK(manager.GetCurrentScreenName() == "temp");
    ATOM_CHECK(manager.UnloadScreen("temp") == atom::ScreenOpResult::InUse);

    // The remaining stacked/registered generation still unloads cleanly.
    ATOM_CHECK(manager.GetScreen("home") == home_generation3_ptr);
    ATOM_CHECK(manager.UnloadScreen("home") == atom::ScreenOpResult::Success);
    ATOM_CHECK(home_counters.destroys == 3);
    ATOM_CHECK(manager.GetScreen("home") == nullptr);

    // --- ARCH-105: re-entrant lifecycle callbacks ---------------------------
    // A screen's OnDeactivate may call back into the manager, which inserts and
    // erases registrations and therefore rehashes the registry. The manager must
    // tolerate that: it snapshots every handle it needs before running a
    // callback and never dereferences a container iterator afterwards. (The
    // previous implementation read `it->second` after the callback, which is a
    // use-after-invalidation; how loudly that misbehaves is allocator-dependent,
    // so this test asserts the documented outcome instead of a crash.)
    {
        ScreenCounters reentrant_counters{};
        auto reentrant = std::make_unique<CountingScreen>("reent-a", &reentrant_counters);
        auto* reentrant_ptr = reentrant.get();
        auto reent_b_owned = std::make_unique<CountingScreen>("reent-b");
        auto* reent_b = reent_b_owned.get();
        ATOM_CHECK(manager.LoadScreen("reent-a", std::move(reentrant)) == atom::ScreenOpResult::Success);
        ATOM_CHECK(manager.LoadScreen("reent-b", std::move(reent_b_owned)) == atom::ScreenOpResult::Success);
        ATOM_CHECK(manager.SwitchScreen("reent-a") == atom::ScreenOpResult::Success);

        reentrant_ptr->on_deactivate = [&manager] {
            // Enough churn to force several rehashes of the registry.
            for (int i = 0; i < 64; ++i)
                static_cast<void>(manager.LoadScreen("reent-scratch-" + std::to_string(i),
                                                     std::make_unique<CountingScreen>("reent-scratch")));
            for (int i = 0; i < 64; ++i)
                static_cast<void>(manager.UnloadScreen("reent-scratch-" + std::to_string(i)));
        };

        ATOM_CHECK(manager.SwitchScreen("reent-b") == atom::ScreenOpResult::Success);
        ATOM_CHECK(manager.GetCurrentScreenName() == "reent-b");
        ATOM_CHECK(manager.GetCurrentScreen() == reent_b);
        ATOM_CHECK(manager.GetScreen("reent-b") == reent_b);
        ATOM_CHECK(manager.IsScreenVisible(reent_b));
        ATOM_CHECK(manager.GetScreen("reent-scratch-0") == nullptr);
        ATOM_CHECK(manager.GetScreen("reent-scratch-63") == nullptr);
        ATOM_CHECK(!manager.IsScreenVisible(reentrant_ptr)); // the switch cleared the stack

        // Same again through LoadScreen(Replace) on the active screen: the
        // replacement must end up active even though the callback rehashed.
        auto replacement = std::make_unique<CountingScreen>("reent-b", &reentrant_counters);
        auto* replacement_ptr = replacement.get();
        reent_b->on_deactivate = [&manager] {
            for (int i = 0; i < 64; ++i)
                static_cast<void>(manager.LoadScreen("reent-scratch-" + std::to_string(i),
                                                     std::make_unique<CountingScreen>("reent-scratch")));
            for (int i = 0; i < 64; ++i)
                static_cast<void>(manager.UnloadScreen("reent-scratch-" + std::to_string(i)));
        };
        ATOM_CHECK(manager.LoadScreen("reent-b", std::move(replacement), atom::ScreenReplacePolicy::Replace) ==
                   atom::ScreenOpResult::Success);
        ATOM_CHECK(manager.GetScreen("reent-b") == replacement_ptr);
        ATOM_CHECK(manager.GetCurrentScreen() == replacement_ptr);
        ATOM_CHECK(replacement_ptr->activates == 1);

        ATOM_CHECK(manager.UnloadScreen("reent-a") == atom::ScreenOpResult::Success);
    }

    return 0;
}
