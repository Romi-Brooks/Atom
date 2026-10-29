/**
 * @file SliceSelfTest.cpp
 * @brief Headless verification of the vertical slice (`Example_2D_Smoke --selftest`).
 *
 * The slice is a pile of already-tested pieces, so what is worth checking is not each piece but
 * their *assembly*: does the scene document produce a world the systems can drive, does input
 * move the player through physics, does the sensor exit actually fire, and does a restart come
 * back clean.
 *
 * It runs without a window on purpose. A GPU is not needed to answer any of those questions, and
 * a test that needs a device is a test that only runs on a desktop - which is exactly the
 * "builds everywhere, verified nowhere" trap the plan warns about.
 */

#include "SliceSelfTest.hpp"

#include <cstdint>
#include <string>

#include <Backend/Contracts/Window/IWindow.hpp>
#include <ECS/Components.hpp>
#include <Input/InputSystem.hpp>
#include <Log/LogSystem.hpp>

#include "GameInput.hpp"
#include "SmokeScreen.hpp"

namespace atom::game {

namespace {

using atom::input::InputSystem;
using atom::input::Key;
using atom::window::EventType;
using atom::window::IEvent;
using atom::window::KeyEvent;

auto MakeKey(const EventType type, const Key key) -> IEvent {
    IEvent event{};
    event.type = type;
    event.data = KeyEvent{key};
    return event;
}

// One fixed step of the slice: publish the frame budget, then let the screen run its ordered
// world frame. This is the same call sequence Application performs, minus the window and GPU.
auto StepFrame(SmokeScreen& screen) -> void {
    screen.BeginFrame(1, 1.0F / 60.0F, 0.0F);
    screen.Update(1.0F / 60.0F);
}

auto PlayerPosition(SmokeScreen& screen) -> algo::Vec2 {
    auto& entities = screen.World().GetEntities();
    const auto player = FindBySceneId(entities, kPlayerSceneId);
    if (!player.IsValid())
        return {};
    const auto* transform = entities.TryGet<ecs::Transform2D>(player);
    return transform != nullptr ? transform->position : algo::Vec2{};
}

auto Fail(const std::string_view message) -> int {
    LOG_ERROR(atom::log::core::Main, "slice self-test FAILED: " + std::string{message});
    return 1;
}

} // namespace

auto RunSliceSelfTest() -> int {
    InputSystem::GetInstance().Reset();
    GameInput::GetInstance().ApplyDefaultBindings();

    SmokeScreen screen{};
    if (auto loaded = screen.LoadRoom(); !loaded.has_value())
        return Fail("the room did not load: " + loaded.error().Message());

    // --- the document produced the world the slice expects ----------------
    {
        auto& entities = screen.World().GetEntities();
        if (!FindBySceneId(entities, kPlayerSceneId).IsValid())
            return Fail("the player entity is missing");
        if (!FindBySceneId(entities, kExitSceneId).IsValid())
            return Fail("the exit entity is missing");
    }

    // Two frames with no input. Measured *after* stepping, because bodies are created by the
    // first Step (create-before-step), not by the load: the count is 0 until a frame has run.
    const auto rest = PlayerPosition(screen);
    StepFrame(screen);
    StepFrame(screen);

    if (const auto drifted = PlayerPosition(screen); (drifted - rest).Length() > 0.5F)
        return Fail("the player drifted with no input");
    // 8 bodies: player, four walls, two crates, exit sensor. The count is the cheapest way to
    // notice that a scene edit silently dropped one.
    if (screen.BodyCount() != 8)
        return Fail("expected 8 physics bodies, got " + std::to_string(screen.BodyCount()));
    if (Slice().contact_count != 0)
        return Fail("the room starts with overlapping bodies (" + std::to_string(Slice().contact_count) + " contacts)");

    // --- input moves the player through the solver ------------------------
    //
    // Held D for 30 fixed steps: 220 px/s * 0.5 s = 110 px expected. The assertion is loose
    // because a solver is not required to be exact, but it must be the right order of magnitude.
    {
        InputSystem::GetInstance().ProcessEvent(MakeKey(EventType::KeyPressed, Key::D));
        for (int step = 0; step < 30; ++step)
            StepFrame(screen);
        InputSystem::GetInstance().ProcessEvent(MakeKey(EventType::KeyReleased, Key::D));

        const auto moved = PlayerPosition(screen);
        const float delta_x = moved.GetX() - rest.GetX();
        if (delta_x < 60.0F || delta_x > 160.0F)
            return Fail("holding right for 0.5 s moved the player " + std::to_string(delta_x) + " px");
        if (moved.GetY() - rest.GetY() > 1.0F)
            return Fail("moving right also moved the player vertically");
    }

    // --- the sensor exit fires when it is reached -------------------------
    //
    // The exit sits at x = 736 and the player starts at 440, so walking right must reach it.
    {
        InputSystem::GetInstance().ProcessEvent(MakeKey(EventType::KeyPressed, Key::D));
        bool reached = false;
        for (int step = 0; step < 240 && !reached; ++step) {
            StepFrame(screen);
            reached = Slice().reached_exit;
        }
        InputSystem::GetInstance().ProcessEvent(MakeKey(EventType::KeyReleased, Key::D));

        if (!reached)
            return Fail("walking into the exit sensor never fired");
        if (Slice().contact_count == 0)
            return Fail("the exit fired without any contact being observed");
    }

    // --- restart returns to a clean state --------------------------------
    if (auto reloaded = screen.LoadRoom(); !reloaded.has_value())
        return Fail("restart failed: " + reloaded.error().Message());
    // Same reason as above: bodies exist after the first Step, so a frame has to run before the
    // count means anything - and that frame is also what would reveal a leftover body.
    StepFrame(screen);
    if (Slice().reached_exit || Slice().contact_count != 0)
        return Fail("restart kept the previous run's state");
    if (screen.BodyCount() != 8)
        return Fail("restart left " + std::to_string(screen.BodyCount()) + " bodies instead of 8");

    // --- pause closes the gameplay gate but not the menu ------------------
    {
        const auto before = PlayerPosition(screen);
        screen.SetPaused(true);
        InputSystem::GetInstance().ProcessEvent(MakeKey(EventType::KeyPressed, Key::D));
        for (int step = 0; step < 10; ++step)
            StepFrame(screen);
        InputSystem::GetInstance().ProcessEvent(MakeKey(EventType::KeyReleased, Key::D));
        if ((PlayerPosition(screen) - before).Length() > 0.5F)
            return Fail("the player moved while paused");
        screen.SetPaused(false);
    }

    LOG_INFO(atom::log::core::Main, "slice self-test passed");
    return 0;
}

} // namespace atom::game
