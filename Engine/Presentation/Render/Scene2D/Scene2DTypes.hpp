// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file Scene2DTypes.hpp
 * @brief Value types and the one seam the Scene2D render layering needs.
 *
 * Everything here is GPU-free on purpose. The collector that turns ECS entities
 * into draw items only needs to know "which textures can I name, and what did I
 * get for this URI", which is why it can be tested headlessly with a stub
 * resolver while the real one is backed by TextureCache.
 *
 * The camera rule (G0.3) is stated once here and nowhere else:
 *
 *   Renderer2D maps world -> screen as `screen = (world - origin) * zoom`.
 *   Put `screen = viewport / 2` at the camera centre and solve:
 *
 *       origin = center - viewport / (2 * zoom)
 *
 *   so CameraView::Origin() is the only conversion, and Camera2D::rotation is
 *   NOT supported in P0 (Renderer2D has no rotated view transform).
 */

#ifndef ATOM_RENDER_SCENE2D_SCENE2DTYPES_HPP
#define ATOM_RENDER_SCENE2D_SCENE2DTYPES_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <Algorithm/Color/Color.hpp>
#include <Algorithm/Geometry/Rect.hpp>
#include <Algorithm/Vector/Vec2.hpp>
#include <ECS/Entity.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

namespace atom::render::scene2d {

/**
 * @brief Resolves an asset URI to a renderer-owned texture.
 *
 * Abstract so SpriteCollector stays GPU-free: production code passes a
 * TextureCache-backed resolver, tests pass a map of stubs.
 *
 * Contract: callers may call this once per sprite per frame, so an
 * implementation must memoise both hits and misses (returning nullptr for a URI
 * that failed once, instead of touching the filesystem again).
 */
class ITextureResolver {
    public:
        virtual ~ITextureResolver() = default;

        // nullptr means "not available"; the collector skips the sprite and
        // counts it, rather than drawing a placeholder the game did not ask for.
        [[nodiscard]] virtual auto Resolve(std::string_view uri) -> Renderer2D::Texture* = 0;
};

/**
 * @brief The camera lens applied to a whole sprite pass.
 *
 * `center` and `viewport` are in world pixels; zoom is a uniform scale. Rotation
 * is deliberately absent: see the note at the top of this header.
 */
struct CameraView {
        algo::Vec2 center{};
        algo::Vec2 viewport{};
        float zoom = 1.0f;

        // World coordinate that Renderer2D must treat as screen (0,0).
        [[nodiscard]] auto Origin() const -> algo::Vec2 {
            if (!IsUsable())
                return center;
            const float half_width = viewport.GetX() * 0.5f / zoom;
            const float half_height = viewport.GetY() * 0.5f / zoom;
            return {center.GetX() - half_width, center.GetY() - half_height};
        }

        // A zero/negative/NaN zoom would collapse or invert the whole pass, so it
        // is rejected rather than clamped silently: the caller gets a diagnostic
        // and draws nothing, which is easier to notice than an invisible scene.
        [[nodiscard]] auto IsUsable() const -> bool;
};

/**
 * @brief One resolved sprite, ready to submit.
 *
 * `destination` is already in world pixels with the pivot applied; `source` is
 * the texel rectangle (valid only when `has_source`).
 *
 * `layer` and `sort_key` are carried so the order can be asserted and
 * re-sorted, but the vector itself is kept sorted: submission order is what the
 * renderer preserves within a layer.
 */
struct SpriteDraw {
        Renderer2D::Texture* texture = nullptr;
        algo::Rect destination{};
        algo::Rect source{};
        bool has_source = false;
        color::Color tint{};
        bool flip_x = false;
        bool flip_y = false;
        // Carried per draw because the renderer's batching groups by sampler: mixing
        // Nearest and Linear sprites costs extra batches, which is why the choice comes
        // from the atlas rather than being made per entity.
        render::Filter2D filter = render::Filter2D::Linear;
        std::int32_t layer = 0;
        std::int32_t sort_key = 0;
        ecs::Entity entity{};
};

/**
 * @brief What one collect pass produced, for diagnostics and tests.
 */
struct CollectStats {
        std::size_t considered = 0;  // sprites seen in the world
        std::size_t drawn = 0;       // sprites with a resolved texture
        std::size_t unresolved = 0;  // skipped: URI missing or resolver returned null
        std::size_t degenerate = 0;  // skipped: zero/NaN size after pivot and scale
};

} // namespace atom::render::scene2d

#endif // ATOM_RENDER_SCENE2D_SCENE2DTYPES_HPP
