// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file World2DRenderer.hpp
 * @brief One ordered draw pass for tile layers, ECS sprites and particles.
 */
#ifndef ATOM_WORLD2D_WORLD2DRENDERER_HPP
#define ATOM_WORLD2D_WORLD2DRENDERER_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Atom/Presentation/Render/Scene2D/Scene2DRenderer.hpp>
#include <Atom/World/World2D/ParticleSystem2D.hpp>
#include <Atom/World/World2D/Tilemap.hpp>

#include <Atom/Build/GraphicsExport.hpp>

namespace atom::world2d {

struct WorldDraw2D final {
        render::Renderer2D::Texture* texture = nullptr;
        algo::Rect source{};
        algo::Rect destination{};
        color::Color tint = color::Color::White();
        render::Filter2D filter = render::Filter2D::Linear;
        std::int32_t layer = 0;
        std::int32_t sort_key = 0;
        std::uint8_t kind = 0; // tile=0, sprite=1, particle=2
        std::uint64_t order = 0;
        bool has_source = false;
        bool flip_x = false;
        bool flip_y = false;
};

class ATOM_GRAPHICS_API World2DRenderer final {
    public:
        explicit World2DRenderer(render::scene2d::ITextureResolver& resolver)
            : resolver_(&resolver), sprites_(resolver) {}

        // Tiles are always queried by view. Opt in to clipping sprite and
        // particle draws to the same viewport before sorting the combined list.
        [[nodiscard]] auto Collect(ecs::EntityWorld& world, const render::scene2d::CameraView& view,
                                   const Tilemap* tilemap = nullptr, ParticleSystem2D* particles = nullptr,
                                   float interpolation_alpha = 0.0f, bool cull_to_view = false)
            -> const std::vector<WorldDraw2D>&;
        [[nodiscard]] auto Draws() const noexcept -> const std::vector<WorldDraw2D>& {
            return draws_;
        }
        [[nodiscard]] auto SpriteCount() const noexcept -> std::size_t {
            return sprites_.Stats().drawn;
        }
        [[nodiscard]] auto Submit(render::Renderer2D& renderer, const render::scene2d::CameraView& view) -> bool;

    private:
        render::scene2d::ITextureResolver* resolver_ = nullptr;
        render::scene2d::Scene2DRenderer sprites_;
        std::vector<WorldDraw2D> draws_{};
};

} // namespace atom::world2d

#endif // ATOM_WORLD2D_WORLD2DRENDERER_HPP
