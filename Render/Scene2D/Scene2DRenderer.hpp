// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Scene2DRenderer.hpp
 * @brief Turns ECS sprites into a sorted, camera-ready 2D draw pass.
 *
 * The split is deliberate and is what makes T1.2 testable headlessly:
 *
 *   SnapshotTransforms  simulation -> display copy (RenderTransform2D)
 *   FindActiveCamera    Transform2D + Camera2D -> CameraView
 *   Collect             RenderTransform2D + SpriteRenderer -> sorted SpriteDraw[]
 *   Submit              SpriteDraw[] -> Renderer2D, no decisions left
 *
 * Only Submit touches the GPU. Collect is pure CPU and is what the tests drive.
 *
 * "RenderSystem reads only the display transform" is enforced by Collect reading
 * RenderTransform2D and never Transform2D: a sprite cannot move because the
 * renderer ran, and a renderer cannot accidentally reorder simulation state.
 */

#ifndef ATOM_RENDER_SCENE2D_SCENE2DRENDERER_HPP
#define ATOM_RENDER_SCENE2D_SCENE2DRENDERER_HPP

#include <cstddef>
#include <string_view>
#include <vector>

#include <ECS/EntityWorld.hpp>
#include <Render/Scene2D/Scene2DTypes.hpp>

namespace atom::render::scene2d {

class Scene2DRenderer final {
    public:
        explicit Scene2DRenderer(ITextureResolver& resolver) : resolver_(&resolver) {}

        // --- display state ---------------------------------------------------

        // Copies Transform2D into RenderTransform2D for every entity that owns
        // both, and returns how many it touched. Called once per frame, before
        // Collect. Interpolation between fixed steps is not done here: it needs a
        // previous-state buffer, which is a later concern (see the component doc).
        //
        // Entities with a sprite but no RenderTransform2D are left alone: the
        // collector skips them, so a missing display copy shows as "sprite not
        // drawn" rather than as a phantom drawn from simulated state.
        static auto SnapshotTransforms(ecs::EntityWorld& world) -> std::size_t;

        // The active camera: the lowest-index entity owning both Transform2D and
        // Camera2D. A world with no camera yields a view centred on the origin
        // with the given fallback viewport and zoom 1, so a scene without a camera
        // still renders instead of failing.
        [[nodiscard]] static auto FindActiveCamera(ecs::EntityWorld& world, algo::Vec2 fallback_viewport) -> CameraView;

        // --- collection ------------------------------------------------------

        // Rebuilds the draw list from the world. Returns the list; `stats()`
        // explains what was skipped. Order: layer asc, sort_key asc, entity index
        // asc - stable across runs for the same world state.
        auto Collect(ecs::EntityWorld& world) -> const std::vector<SpriteDraw>&;

        [[nodiscard]] auto Draws() const noexcept -> const std::vector<SpriteDraw>& {
            return draws_;
        }
        [[nodiscard]] auto Stats() const noexcept -> const CollectStats& {
            return stats_;
        }
        // The first URI that failed to resolve, for a one-line diagnostic. Empty
        // when everything resolved.
        [[nodiscard]] auto LastUnresolvedUri() const -> std::string_view {
            return last_unresolved_uri_;
        }

        // --- submission ------------------------------------------------------

        // Opens a Renderer2D frame with the camera view, submits the collected
        // list in order, and closes it. Returns false (and draws nothing) when the
        // view is unusable or the renderer refuses the frame.
        //
        // The list is already sorted and Renderer2D stable-sorts by layer only, so
        // submission order is preserved within a layer: that is why this does not
        // push per-item layers, and why Collect must sort.
        auto Submit(Renderer2D& renderer, const CameraView& view) -> bool;

    private:
        ITextureResolver* resolver_ = nullptr;
        std::vector<SpriteDraw> draws_{};
        CollectStats stats_{};
        std::string last_unresolved_uri_{};
};

} // namespace atom::render::scene2d

#endif // ATOM_RENDER_SCENE2D_SCENE2DRENDERER_HPP
