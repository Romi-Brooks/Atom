/**
 * @file SmokeScreen.hpp
 * @brief T1.3 - a GameScreen with a hard-coded world, real res:// sprites, and a
 *        camera, i.e. the smallest thing that proves T1.1 + T1.2 end to end.
 *
 * Deliberately has no Scene loader: the world is built in code by
 * `SeedSmokeScene`, so a broken Scene schema cannot be blamed for a broken sprite
 * pipeline. T2.2 replaces exactly this function with a loader call.
 *
 * Content root (G0.5): main() mounts the repository's Assets/ directory as
 * res://, so the sprite URIs below are real VFS lookups, not paths.
 */

#ifndef ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP
#define ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <Filesystem/Vfs.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Render/Resources/TextureCache.hpp>
#include <Render/Scene2D/Scene2DRenderer.hpp>
#include <Render/Scene2D/TextureCacheResolver.hpp>
#include <Runtime/GameScreen.hpp>

namespace atom::game {

class SmokeScreen final : public runtime::GameScreen {
    public:
        SmokeScreen();

        ~SmokeScreen() override;

        SmokeScreen(const SmokeScreen&) = delete;
        auto operator=(const SmokeScreen&) -> SmokeScreen& = delete;

        auto Render(render::IRenderDevice& device) -> void override;
        auto HandleEvent(const window::IEvent& event) -> bool override;

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

    protected:
        auto OnWorldFrameAdvanced(float delta_seconds) -> void override;

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

// Builds the smoke scene into a world: a camera, a player, a floor of sprites with
// colliding sort keys, two crates, and one atlas cell to prove the source-rect
// path. Exposed so a test could seed the same world without a window.
[[nodiscard]] auto SeedSmokeScene(runtime::GameWorld& world) -> Result<void>;

} // namespace atom::game

#endif // ATOM_EXAMPLE_GAME_SMOKESCREEN_HPP
