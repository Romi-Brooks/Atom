/**
 * @file SmokeScreen.hpp
 * @brief T2.2 - a GameScreen whose world comes from a scene document.
 *
 * What changed from T1.3: the room's entities (camera, player, crates, atlas cell,
 * expiring marker) are no longer built in C++. They come from
 * `res://scenes/smoke_room.scene.json` through SceneLoader, and the switch into the
 * running world is a single `GameWorld::ReplaceEntities`.
 *
 * Two consequences the code shows explicitly:
 *
 *  - systems cannot cache an Entity handle. A swap replaces the slot table, so a
 *    cached handle can even be "alive" again by accident (the new world's first
 *    entity has the same index+generation); every system looks its entity up by
 *    SceneEntityId instead (`FindBySceneId`).
 *  - the backdrop (the floor grid) is still generated in code. It is decoration, not
 *    scene content: T5.1's Tilemap will generate layers like this from a tileset.
 *
 * Content root (G0.5): main() mounts the repository's Assets/ directory as res://,
 * so the URIs in the scene document are real VFS lookups, not paths.
 */

#ifndef ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP
#define ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <ECS/Entity.hpp>
#include <ECS/EntityWorld.hpp>
#include <Filesystem/Vfs.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Render/Resources/TextureCache.hpp>
#include <Render/Scene2D/Scene2DRenderer.hpp>
#include <Render/Scene2D/TextureCacheResolver.hpp>
#include <Runtime/GameScreen.hpp>
#include <Runtime/GameWorld.hpp>

namespace atom::game {

// Stable ids from Assets/scenes/smoke_room.scene.json: the contract between the
// document and the systems, named once instead of repeated as magic numbers.
inline constexpr std::uint32_t kCameraSceneId = 1;
inline constexpr std::uint32_t kPlayerSceneId = 2;

class SmokeScreen final : public runtime::GameScreen {
    public:
        SmokeScreen();

        ~SmokeScreen() override;

        SmokeScreen(const SmokeScreen&) = delete;
        auto operator=(const SmokeScreen&) -> SmokeScreen& = delete;

        // Loads res://scenes/smoke_room.scene.json and makes it the running world.
        // Called by main before the loop starts. A failure leaves the current world
        // untouched and reports why - the whole point of loading into a temporary
        // world first.
        [[nodiscard]] auto LoadRoom() -> Result<void>;

        auto Render(render::IRenderDevice& device) -> void override;
        auto HandleEvent(const window::IEvent& event) -> bool override;

        // The gameplay gate follows the pause state here rather than in each system: one
        // forgotten `if (paused) return;` is exactly the bug T3.2 exists to remove, and
        // this hook runs every frame even while the world is paused.
        auto BeginFrame(std::uint32_t fixed_steps, float fixed_delta_seconds, float interpolation_alpha) -> void override;

        // Diagnostics for the debugger panel (and for a human running the demo).
        [[nodiscard]] auto SpriteCount() const -> std::size_t {
            return scene_renderer_.Stats().drawn;
        }
        [[nodiscard]] auto SkippedCount() const -> std::size_t {
            return scene_renderer_.Stats().unresolved + scene_renderer_.Stats().degenerate;
        }
        [[nodiscard]] auto LastUnresolvedUri() const -> std::string_view {
            return scene_renderer_.LastUnresolvedUri();
        }
        [[nodiscard]] auto CameraZoom() const noexcept -> float {
            return camera_zoom_;
        }
        [[nodiscard]] auto EntityCount() const -> std::size_t;

    private:
        // Declared in dependency order: the cache views the renderer, the resolver
        // holds cache handles, and the scene renderer needs the resolver.
        render::Renderer2D renderer_{};
        render::resources::TextureCache texture_cache_{renderer_};
        render::scene2d::TextureCacheResolver resolver_{texture_cache_, fs::Vfs::GetInstance()};
        render::scene2d::Scene2DRenderer scene_renderer_{resolver_};

        std::uint32_t window_width_ = 0;
        std::uint32_t window_height_ = 0;
        float camera_zoom_ = 1.0F;
        bool render_failed_ = false;
};

// Looks an entity up by its stable scene id. Systems use this instead of caching a
// handle, because a scene swap invalidates every handle it did not create.
[[nodiscard]] auto FindBySceneId(ecs::EntityWorld& entities, std::uint32_t scene_id) -> ecs::Entity;

// Registers the demo's systems. Called from the GameScreen setup callback, so the
// systems exist before the first frame and survive the scene swap.
[[nodiscard]] auto RegisterSmokeSystems(runtime::GameWorld& world) -> Result<void>;

} // namespace atom::game

#endif // ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP
