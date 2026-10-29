/**
 * @file           : GameScreenFrameTest.cpp
 * @brief          : T1.3 / G0.1 - how a Screen's frame maps onto a GameWorld frame.
 *
 * The host knows two callbacks: FixedUpdate (0..N times) and Update (once). A
 * GameWorld must see one ordered frame: Input, FixedUpdate x N, Update, LateUpdate.
 * This test pins the decided bridge - the host publishes its fixed-step budget in
 * BeginFrame, and the screen batches it into one GameWorld::RunFrame from Update -
 * by recording the phase sequence the world actually executes.
 *
 * No window, no device: Render() is never called, which is exactly the point of
 * keeping the frame-rhythm contract in Runtime rather than in the renderer.
 */

#include <Runtime/GameScreen.hpp>

#include <ECS/Components.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using atom::Result;
using atom::ecs::EntityWorld;
using atom::render::IRenderDevice;
using atom::runtime::FrameContext;
using atom::runtime::GameScreen;
using atom::runtime::GameWorld;
using atom::runtime::SystemDescriptor;
using atom::runtime::SystemPhase;
using atom::window::IEvent;

// Records the phase order the world runs, so the assertion is about what actually
// executed rather than about the call the test made.
struct PhaseLog final {
        std::vector<std::string> phases{};
        std::vector<std::uint32_t> fixed_indices{};
        std::size_t advanced = 0;

        auto Record(const std::string& name) -> void {
            phases.push_back(name);
        }
};

auto AddRecorder(GameWorld& world, const SystemPhase phase, const char* name, PhaseLog& log) -> Result<void> {
    return world.GetScheduler().Add(
        phase, SystemDescriptor{std::string{name}, [&log, phase](GameWorld& /*world*/, const FrameContext& context) {
                   switch (phase) {
                       case SystemPhase::Input:
                           log.Record("input");
                           break;
                       case SystemPhase::FixedUpdate:
                           log.Record("fixed");
                           log.fixed_indices.push_back(context.fixed_step_index);
                           break;
                       case SystemPhase::Update:
                           log.Record("update");
                           break;
                       case SystemPhase::LateUpdate:
                           log.Record("late");
                           break;
                   }
                   return atom::Ok();
               }});
}

class RecordingScreen final : public GameScreen {
    public:
        RecordingScreen(PhaseLog& log, const std::size_t failing_step = 0)
            : GameScreen([&log, failing_step](GameWorld& world) -> Result<void> {
                  if (auto added = AddRecorder(world, SystemPhase::Input, "input", log); !added.has_value())
                      return added;
                  if (auto added = AddRecorder(world, SystemPhase::FixedUpdate, "fixed", log); !added.has_value())
                      return added;
                  if (auto added = AddRecorder(world, SystemPhase::Update, "update", log); !added.has_value())
                      return added;
                  if (auto added = AddRecorder(world, SystemPhase::LateUpdate, "late", log); !added.has_value())
                      return added;
                  if (failing_step != 0) {
                      // Prove a refused setup surfaces as a construction failure
                      // rather than as a half-configured screen.
                      return atom::Fail(atom::MakeError(atom::ErrorCode::Internal, atom::Module::Core,
                                                        "RecordingScreen")
                                            .WithDetail("intentional setup failure"));
                  }
                  return atom::Ok();
              }) {}

        auto Render(IRenderDevice& /*device*/) -> void override {}
        auto HandleEvent(const IEvent& /*event*/) -> bool override {
            return false;
        }

};

auto ExpectSequence(const std::vector<std::string>& actual, const std::vector<std::string>& expected,
                    const char* label) -> int {
    if (actual.size() != expected.size())
        return atom::test::Fail(std::string{label} + ": sequence length mismatch");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (actual[i] != expected[i])
            return atom::test::Fail(std::string{label} + ": phase order mismatch at index " + std::to_string(i));
    }
    return 0;
}

} // namespace

auto main() -> int {
    // --- 1. N fixed steps: Input once, FixedUpdate x N, Update, LateUpdate ---
    {
        PhaseLog log{};
        RecordingScreen screen{log};

        ATOM_CHECK(screen.FrameCount() == 0);
        screen.BeginFrame(3, 0.01F, 0.5F);
        screen.Update(0.033F);

        ATOM_CHECK(ExpectSequence(log.phases, {"input", "fixed", "fixed", "fixed", "update", "late"}, "3 steps") == 0);
        ATOM_CHECK(log.fixed_indices.size() == 3);
        ATOM_CHECK(log.fixed_indices[0] == 0);
        ATOM_CHECK(log.fixed_indices[1] == 1);
        ATOM_CHECK(log.fixed_indices[2] == 2);
        ATOM_CHECK(screen.FrameCount() == 1);
        ATOM_CHECK_NEAR(screen.InterpolationAlpha(), 0.5F);
        // FixedUpdate must never be driven by the host on this screen: the batch in
        // Update is the only dispatcher, so the host calling FixedUpdate directly
        // would run the phases twice. The screen inherits the default no-op, and the
        // sequence above shows no duplicated phases.
    }

    // --- 2. Zero fixed steps: the frame still runs Input/Update/LateUpdate --
    {
        PhaseLog log{};
        RecordingScreen screen{log};

        screen.BeginFrame(0, 0.0F, 0.0F);
        screen.Update(0.016F);

        ATOM_CHECK(ExpectSequence(log.phases, {"input", "update", "late"}, "0 steps") == 0);
        ATOM_CHECK(log.fixed_indices.empty());
        ATOM_CHECK(screen.FrameCount() == 1);
    }

    // --- 3. The budget is consumed, not remembered across frames ----------
    //
    // A screen that replayed the previous frame's step count would silently double
    // the simulation rate whenever the host produced a zero-step frame.
    {
        PhaseLog log{};
        RecordingScreen screen{log};

        screen.BeginFrame(2, 0.01F, 0.25F);
        screen.Update(0.02F);
        log.phases.clear();
        log.fixed_indices.clear();

        // No BeginFrame this time (a host that skipped it, or a direct driver).
        screen.Update(0.02F);
        ATOM_CHECK(ExpectSequence(log.phases, {"input", "update", "late"}, "consumed budget") == 0);
        ATOM_CHECK(log.fixed_indices.empty());
        ATOM_CHECK(screen.FrameCount() == 2);
    }

    // --- 4. Pause stops the world without stopping the host ----------------
    {
        PhaseLog log{};
        RecordingScreen screen{log};

        screen.SetPaused(true);
        ATOM_CHECK(screen.IsPaused());
        screen.BeginFrame(2, 0.01F, 0.25F);
        screen.Update(0.02F);

        ATOM_CHECK(log.phases.empty());
        ATOM_CHECK(screen.FrameCount() == 0);
        // Alpha is still published while paused: the host keeps rendering, and the last
        // two steps it interpolates between are frozen, so the picture stays put rather
        // than dropping to the newest step.
        ATOM_CHECK_NEAR(screen.InterpolationAlpha(), 0.25F);

        screen.SetPaused(false);
        screen.BeginFrame(1, 0.01F, 0.1F);
        screen.Update(0.02F);
        ATOM_CHECK(ExpectSequence(log.phases, {"input", "fixed", "update", "late"}, "resumed") == 0);
        ATOM_CHECK(screen.FrameCount() == 1);
    }

    // --- 4b. Alpha is clamped into [0, 1) --------------------------------
    //
    // A clock that has not advanced yet can report 0, and a rounding wobble can report
    // something just above 1. Interpolating with either would extrapolate past the
    // committed state, so the screen clamps.
    {
        PhaseLog log{};
        RecordingScreen screen{log};

        screen.BeginFrame(1, 0.01F, -0.5F);
        ATOM_CHECK_NEAR(screen.InterpolationAlpha(), 0.0F);
        screen.BeginFrame(1, 0.01F, 1.5F);
        ATOM_CHECK_NEAR(screen.InterpolationAlpha(), 0.999F);
    }

    // --- 5. The world is real: entities created in Input survive the frame -
    //
    // This is the smoke-scene shape T1.3 needs - spawn in the Input phase, read in
    // Update - so it is asserted here rather than only in the example.
    {
        struct Spawner final {
                static auto Run(GameWorld& world, const FrameContext& context) -> Result<void> {
                    if (context.phase != SystemPhase::Input)
                        return atom::Ok();
                    world.GetCommands().QueueCreate([](EntityWorld& entities, const atom::ecs::Entity entity) {
                        return entities.Emplace<atom::ecs::Transform2D>(entity,
                                                                       atom::ecs::Transform2D{.position = {7.0F, 9.0F}});
                    });
                    return atom::Ok();
                }
        };

        struct SpawnScreen final : GameScreen {
                SpawnScreen() : GameScreen([](GameWorld& world) -> Result<void> {
                                    if (auto added = world.GetScheduler().Add(
                                            SystemPhase::Input,
                                            SystemDescriptor{"spawn", &Spawner::Run});
                                        !added.has_value())
                                        return added;
                                    return atom::Ok();
                                }) {}
                auto Render(IRenderDevice& /*device*/) -> void override {}
                auto HandleEvent(const IEvent& /*event*/) -> bool override {
                    return false;
                }
        };

        SpawnScreen screen{};
        screen.BeginFrame(1, 0.01F, 0.1F);
        screen.Update(0.016F);

        ATOM_CHECK(screen.FrameCount() == 1);
        ATOM_CHECK(screen.World().GetEntities().CountOf<atom::ecs::Transform2D>() == 1);
        const auto spawned = screen.World().GetEntities().With<atom::ecs::Transform2D>();
        ATOM_CHECK(spawned.size() == 1);
        const auto* transform = screen.World().GetEntities().TryGet<atom::ecs::Transform2D>(spawned.front());
        ATOM_CHECK(transform != nullptr);
        ATOM_CHECK_NEAR(transform->position.GetX(), 7.0F);
    }

    // --- 6. A refused setup fails construction, it does not degrade --------
    {
        PhaseLog log{};
        bool threw = false;
        try {
            RecordingScreen screen{log, 1};
            (void)screen;
        } catch (const std::runtime_error&) {
            threw = true;
        }
        ATOM_CHECK(threw);
    }

    return 0;
}
