/**
 * @file SmokeScreen.hpp
 * @brief The first vertical slice (T3.3): a playable room built from a scene document.
 *
 * What it assembles, all of which already existed as tested pieces:
 *
 *   scene document   the room's entities, prefab instances, atlas frames and physics shells
 *   physics bridge   dynamic player + crates, static walls, one sensor exit
 *   input actions    movement, pause, restart - no system reads a Key
 *   interpolation    drawn between fixed steps, so movement is smooth at any refresh rate
 *   HUD              text drawn by Renderer2D, showing state and controls
 *
 * Deliberate non-goals: no menu screen, no save, no second level, no audio. None of those is
 * needed to answer the only question this slice asks - can the pieces be assembled into
 * something that starts, is played, pauses, restarts and exits?
 */

#ifndef ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP
#define ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <ECS/Entity.hpp>
#include <ECS/EntityWorld.hpp>
#include <Filesystem/Vfs.hpp>
#include <Physics/Integration/Physics2DWorld.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Render/Resources/TextureCache.hpp>
#include <Render/Scene2D/Scene2DRenderer.hpp>
#include <Render/Scene2D/TextureCacheResolver.hpp>
#include <Runtime/GameScreen.hpp>
#include <Runtime/GameWorld.hpp>

namespace atom::game {

// Stable ids from Assets/scenes/smoke_room.scene.json: the contract between the document and
// the systems, named once instead of repeated as magic numbers.
inline constexpr std::uint32_t kCameraSceneId = 1;
inline constexpr std::uint32_t kPlayerSceneId = 2;
inline constexpr std::uint32_t kExitSceneId = 20;

class SmokeScreen final : public runtime::GameScreen {
    public:
        SmokeScreen();

        ~SmokeScreen() override;

        SmokeScreen(const SmokeScreen&) = delete;
        auto operator=(const SmokeScreen&) -> SmokeScreen& = delete;

        // Loads the room and rebuilds the physics world for it. Called at boot and on restart;
        // a failure leaves the current world untouched and reports why.
        [[nodiscard]] auto LoadRoom() -> Result<void>;

        auto Render(render::IRenderDevice& device) -> void override;
        auto HandleEvent(const window::IEvent& event) -> bool override;
        auto BeginFrame(std::uint32_t fixed_steps, float fixed_delta_seconds, float interpolation_alpha)
            -> void override;

        // Diagnostics for the debugger panel (and for a human running the demo).
        [[nodiscard]] auto SpriteCount() const -> std::size_t {
            return scene_renderer_.Stats().drawn;
        }
        [[nodiscard]] auto EntityCount() const -> std::size_t;
        [[nodiscard]] auto BodyCount() const -> std::size_t;

    private:
        auto DrawHud() -> void;
        auto EnsureHudFont() -> render::Font*;

        // Declared in dependency order: the cache views the renderer, the resolver holds cache
        // handles, and the scene renderer needs the resolver.
        render::Renderer2D renderer_{};
        render::resources::TextureCache texture_cache_{renderer_};
        render::scene2d::TextureCacheResolver resolver_{texture_cache_, fs::Vfs::GetInstance()};
        render::scene2d::Scene2DRenderer scene_renderer_{resolver_};


        render::Font* hud_font_ = nullptr;
        std::string hud_font_error_{};

        std::uint32_t window_width_ = 0;
        std::uint32_t window_height_ = 0;
        float camera_zoom_ = 1.0F;
        bool render_failed_ = false;
};

// Looks an entity up by its stable scene id. Systems use this instead of caching a handle,
// because a scene swap invalidates every handle it did not create.
[[nodiscard]] auto FindBySceneId(ecs::EntityWorld& entities, std::uint32_t scene_id) -> ecs::Entity;

// Registers the slice's systems. Called from the GameScreen setup callback, so they exist
// before the first frame and survive the scene swap.
[[nodiscard]] auto RegisterSmokeSystems(runtime::GameWorld& world) -> Result<void>;

// The slice's small piece of game state, shared by the systems and read by the HUD.
struct SliceState {
        bool reached_exit = false;
        std::uint32_t contact_count = 0;

        // Called on restart so a new run does not inherit the previous one's success.
        auto Reset() -> void {
            reached_exit = false;
            contact_count = 0;
        }
};

// One instance for the example: the systems are plain lambdas registered on a world, so they
// cannot capture the screen's members without a construction-order hazard (the base
// GameScreen constructor runs before any member is initialised). A file-scope state keeps that
// hazard out of the design instead of ordering members around it.
[[nodiscard]] auto Slice() -> SliceState&;

} // namespace atom::game

#endif // ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP
