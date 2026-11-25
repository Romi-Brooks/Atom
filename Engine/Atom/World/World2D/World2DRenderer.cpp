// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Atom/World/World2D/World2DRenderer.hpp>

#include <algorithm>
#include <cmath>

namespace atom::world2d {

auto World2DRenderer::Collect(ecs::EntityWorld& world, const render::scene2d::CameraView& view, const Tilemap* tilemap,
                              ParticleSystem2D* particles, const float interpolation_alpha, const bool cull_to_view)
    -> const std::vector<WorldDraw2D>& {
    draws_.clear();
    if (!view.IsUsable())
        return draws_;
    const auto origin = view.Origin();
    const float visible_width = view.viewport.GetX() / view.zoom;
    const float visible_height = view.viewport.GetY() / view.zoom;
    const float left = origin.GetX();
    const float top = origin.GetY();
    const float right = left + visible_width;
    const float bottom = top + visible_height;
    if (cull_to_view && (!std::isfinite(right) || !std::isfinite(bottom) || right <= left || bottom <= top))
        return draws_;

    const auto intersects_view = [&](const algo::Rect& bounds) {
        return !cull_to_view || (bounds.x + bounds.width > left && bounds.x < right && bounds.y + bounds.height > top &&
                                 bounds.y < bottom);
    };
    std::uint64_t order = 0;
    if (tilemap != nullptr && tilemap->IsValid()) {
        for (std::size_t layer = 0; layer < tilemap->TileLayers().size(); ++layer) {
            const auto& info = tilemap->TileLayers()[layer];
            if (!info.visible)
                continue;
            const float offset_x = view.center.GetX() * (1.0f - info.parallax.GetX());
            const float offset_y = view.center.GetY() * (1.0f - info.parallax.GetY());
            const algo::Rect visible{origin.GetX() - offset_x, origin.GetY() - offset_y, visible_width, visible_height};
            tilemap->ForEachCell(
                layer, visible, [&](const std::int32_t x, const std::int32_t y, const TileDefinition& tile) {
                    auto* texture = resolver_->Resolve(tile.texture_uri);
                    if (texture == nullptr)
                        return;
                    draws_.push_back(
                        WorldDraw2D{.texture = texture,
                                    .source = tile.source,
                                    .destination = {static_cast<float>(x) * tilemap->TileSize().GetX() + offset_x,
                                                    static_cast<float>(y) * tilemap->TileSize().GetY() + offset_y,
                                                    tilemap->TileSize().GetX(), tilemap->TileSize().GetY()},
                                    .filter = tile.filter == ecs::SpriteFilter::Nearest ? render::Filter2D::Nearest
                                                                                        : render::Filter2D::Linear,
                                    .layer = info.draw_order,
                                    .kind = 0,
                                    .order = order++,
                                    .has_source = true});
                });
        }
    }

    for (const auto& sprite : sprites_.Collect(world, interpolation_alpha)) {
        if (!intersects_view(sprite.destination))
            continue;
        draws_.push_back(WorldDraw2D{.texture = sprite.texture,
                                     .source = sprite.source,
                                     .destination = sprite.destination,
                                     .tint = sprite.tint,
                                     .filter = sprite.filter,
                                     .layer = sprite.layer,
                                     .sort_key = sprite.sort_key,
                                     .kind = 1,
                                     .order = order++,
                                     .has_source = sprite.has_source,
                                     .flip_x = sprite.flip_x,
                                     .flip_y = sprite.flip_y});
    }
    if (particles != nullptr) {
        for (const auto& particle : particles->Collect(false)) {
            if (!intersects_view(particle.destination))
                continue;
            auto* texture = resolver_->Resolve(particles->PresetTextureUri(particle.preset_index));
            if (texture == nullptr)
                continue;
            draws_.push_back(WorldDraw2D{.texture = texture,
                                         .source = particle.source,
                                         .destination = particle.destination,
                                         .tint = particle.tint,
                                         .filter = particle.filter == ecs::SpriteFilter::Nearest
                                                       ? render::Filter2D::Nearest
                                                       : render::Filter2D::Linear,
                                         .layer = particle.layer,
                                         .kind = 2,
                                         .order = order++,
                                         .has_source = true});
        }
    }
    std::sort(draws_.begin(), draws_.end(), [](const WorldDraw2D& a, const WorldDraw2D& b) {
        if (a.layer != b.layer)
            return a.layer < b.layer;
        if (a.sort_key != b.sort_key)
            return a.sort_key < b.sort_key;
        if (a.kind != b.kind)
            return a.kind < b.kind;
        return a.order < b.order;
    });
    return draws_;
}

auto World2DRenderer::Submit(render::Renderer2D& renderer, const render::scene2d::CameraView& view) -> bool {
    if (!view.IsUsable())
        return false;
    const auto origin = view.Origin();
    if (!renderer.BeginFrame(origin.GetX(), origin.GetY(), view.zoom))
        return false;
    for (const auto& draw : draws_) {
        if (draw.texture == nullptr)
            continue;
        renderer.DrawTexture(*draw.texture, draw.destination, draw.tint, draw.has_source ? &draw.source : nullptr,
                             draw.flip_x, draw.flip_y, draw.filter);
    }
    return renderer.EndFrame();
}

} // namespace atom::world2d
