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

        // Copies Transform2D into RenderTransform2D for every entity that owns both, and
        // returns how many it touched.
        //
        // Run this once per *fixed step*, as the first FixedUpdate system - not once per
        // frame. Done that way, RenderTransform2D is by construction "the state at the
        // previous step", and Collect(alpha) can blend it with Transform2D ("the state at
        // this step"). Done once per frame it would just be a copy of the newest state,
        // and rendering would snap between steps.
        static auto SnapshotTransforms(ecs::EntityWorld& world) -> std::size_t;

        // The active camera: the lowest-index entity owning both Transform2D and
        // Camera2D. A world with no camera yields a view centred on the origin with the
        // given fallback viewport and zoom 1, so a scene without a camera still renders
        // instead of failing.
        //
        // `interpolation_alpha` blends the camera's display state with its simulated one,
        // for the same reason sprites do: a camera that snaps while sprites interpolate
        // makes the whole world jitter.
        [[nodiscard]] static auto FindActiveCamera(ecs::EntityWorld& world, algo::Vec2 fallback_viewport,
                                                   float interpolation_alpha = 0.0F) -> CameraView;

        // --- collection ------------------------------------------------------

        // Rebuilds the draw list from the world, blending each sprite's display state
        // (RenderTransform2D) with its simulated state (Transform2D) by `alpha`. Returns
        // the list; `stats()` explains what was skipped. Order: layer asc, sort_key asc,
        // entity index asc - stable across runs for the same world state.
        //
        // Reading Transform2D here does not break the "render never writes simulation"
        // rule: the blend is computed into the draw item, so neither component is
        // modified. The alternative - writing the blended value back into
        // RenderTransform2D - would corrupt the previous-step state and make the next
        // frame interpolate from an already-interpolated value.
        auto Collect(ecs::EntityWorld& world, float interpolation_alpha = 0.0F) -> const std::vector<SpriteDraw>&;

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
