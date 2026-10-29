// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Render/Scene2D/Scene2DRenderer.hpp>

#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <utility>

#include <ECS/Components.hpp>

namespace atom::render::scene2d {

namespace {

using ecs::Entity;
using ecs::RenderTransform2D;
using ecs::SpriteRenderer;
using ecs::Transform2D;
using ecs::Camera2D;

constexpr std::size_t kMaxDeadUrisRemembered = 1;

auto IsFinitePositive(const float value) -> bool {
    return std::isfinite(value) && value > 0.0f;
}

// --- snapshot ---------------------------------------------------------------

struct SnapshotContext final {
        std::size_t touched = 0;
};

auto SnapshotOne(void* context, Entity /*entity*/, std::span<Transform2D> simulated,
                 std::span<RenderTransform2D> display) -> void {
    display[0].position = simulated[0].position;
    display[0].rotation = simulated[0].rotation;
    display[0].scale = simulated[0].scale;
    static_cast<SnapshotContext*>(context)->touched += 1;
}

// --- collect ----------------------------------------------------------------

struct CollectContext final {
        ITextureResolver* resolver = nullptr;
        std::vector<SpriteDraw>* draws = nullptr;
        CollectStats* stats = nullptr;
        std::string* last_unresolved = nullptr;
};

auto CollectOne(void* context, const Entity entity, std::span<RenderTransform2D> display,
                std::span<SpriteRenderer> sprite) -> void {
    auto& ctx = *static_cast<CollectContext*>(context);
    auto& stats = *ctx.stats;
    stats.considered += 1;

    const auto& render = display[0];
    const auto& data = sprite[0];

    if (data.texture_uri.empty()) {
        stats.unresolved += 1;
        return;
    }
    auto* texture = ctx.resolver->Resolve(data.texture_uri);
    if (texture == nullptr) {
        stats.unresolved += 1;
        if (ctx.last_unresolved->empty())
            *ctx.last_unresolved = data.texture_uri;
        return;
    }

    // Which pixel size does the quad occupy before scale? Explicit size wins, then
    // the atlas sub-rect (so 1 texel maps to 1 pixel at scale 1), then the
    // texture's own dimensions for a whole-texture sprite.
    float width = data.size.GetX();
    float height = data.size.GetY();
    if (!IsFinitePositive(width) || !IsFinitePositive(height)) {
        width = data.source_size.GetX();
        height = data.source_size.GetY();
    }
    if (!IsFinitePositive(width) || !IsFinitePositive(height)) {
        width = static_cast<float>(texture->GetWidth());
        height = static_cast<float>(texture->GetHeight());
    }

    const float scaled_width = width * render.scale.GetX();
    const float scaled_height = height * render.scale.GetY();
    if (!IsFinitePositive(scaled_width) || !IsFinitePositive(scaled_height)) {
        // A zero or NaN size can only produce a degenerate quad or NaN vertices;
        // skipping it is what keeps the vertex stream clean (ARCH-006).
        stats.degenerate += 1;
        return;
    }

    SpriteDraw draw{};
    draw.texture = texture;
    draw.destination = algo::Rect{render.position.GetX() - data.pivot.GetX() * scaled_width,
                                  render.position.GetY() - data.pivot.GetY() * scaled_height, scaled_width,
                                  scaled_height};

    // has_source requires a real texel rect; a zero-size rect means "whole texture"
    // and must not be forwarded as a 0x0 sub-rect.
    if (IsFinitePositive(data.source_size.GetX()) && IsFinitePositive(data.source_size.GetY())) {
        draw.source = algo::Rect{data.source_position.GetX(), data.source_position.GetY(), data.source_size.GetX(),
                                 data.source_size.GetY()};
        draw.has_source = true;
    }

    draw.tint = data.tint;
    draw.flip_x = data.flip_x;
    draw.flip_y = data.flip_y;
    draw.filter = data.filter == ecs::SpriteFilter::Nearest ? render::Filter2D::Nearest : render::Filter2D::Linear;
    draw.layer = data.layer;
    draw.sort_key = data.sort_key;
    draw.entity = entity;

    ctx.draws->push_back(std::move(draw));
    stats.drawn += 1;
}

// Order is layer, then sort_key, then entity index. The index tie-break is what
// makes two sprites with identical keys keep a reproducible order across runs and
// platforms; the sort itself is stable so an even field order never changes.
auto DrawOrderIsLess(const SpriteDraw& left, const SpriteDraw& right) -> bool {
    if (left.layer != right.layer)
        return left.layer < right.layer;
    if (left.sort_key != right.sort_key)
        return left.sort_key < right.sort_key;
    return left.entity.Index() < right.entity.Index();
}

} // namespace

auto CameraView::IsUsable() const -> bool {
    return std::isfinite(zoom) && zoom > 0.0f && std::isfinite(center.GetX()) && std::isfinite(center.GetY()) &&
           std::isfinite(viewport.GetX()) && std::isfinite(viewport.GetY());
}

auto Scene2DRenderer::SnapshotTransforms(ecs::EntityWorld& world) -> std::size_t {
    SnapshotContext context{};
    if (auto result = world.ForEach<Transform2D, RenderTransform2D>({&SnapshotOne, &context}); !result.has_value())
        return 0;
    return context.touched;
}

auto Scene2DRenderer::FindActiveCamera(ecs::EntityWorld& world, const algo::Vec2 fallback_viewport) -> CameraView {
    CameraView view{};
    view.center = algo::Vec2::Zero();
    view.viewport = fallback_viewport;
    view.zoom = 1.0f;

    // With<> returns ascending entity index, so the first usable camera is the
    // lowest-index one: "one active camera" without a second tag.
    for (const auto entity : world.With<Transform2D, Camera2D>()) {
        const auto* transform = world.TryGet<Transform2D>(entity);
        const auto* lens = world.TryGet<Camera2D>(entity);
        if (transform == nullptr || lens == nullptr)
            continue;

        view.center = transform->position;
        // A non-positive zoom is a scene bug; fall back to 1 so the pass still
        // renders, and leave IsUsable() true. Rejecting the whole camera would
        // hide the scene behind a blank screen.
        view.zoom = IsFinitePositive(lens->zoom) ? lens->zoom : 1.0f;
        const bool viewport_given = IsFinitePositive(lens->viewport_size.GetX()) &&
                                    IsFinitePositive(lens->viewport_size.GetY());
        if (viewport_given)
            view.viewport = lens->viewport_size;
        return view;
    }
    return view;
}

auto Scene2DRenderer::Collect(ecs::EntityWorld& world) -> const std::vector<SpriteDraw>& {
    // Reserve nothing extra: the vector keeps its capacity across frames, and a
    // scene that shrinks should not pin memory.
    draws_.clear();
    stats_ = CollectStats{};
    last_unresolved_uri_.clear();

    CollectContext context{};
    context.resolver = resolver_;
    context.draws = &draws_;
    context.stats = &stats_;
    context.last_unresolved = &last_unresolved_uri_;

    if (resolver_ == nullptr) {
        // Without a resolver nothing can be drawn; report it as "everything
        // unresolved" instead of pretending the scene is empty.
        stats_.considered = world.CountOf<RenderTransform2D>();
        stats_.unresolved = stats_.considered;
        return draws_;
    }

    if (auto result = world.ForEach<RenderTransform2D, SpriteRenderer>({&CollectOne, &context}); !result.has_value()) {
        draws_.clear();
        stats_ = CollectStats{};
        return draws_;
    }

    std::stable_sort(draws_.begin(), draws_.end(), DrawOrderIsLess);
    return draws_;
}

auto Scene2DRenderer::Submit(Renderer2D& renderer, const CameraView& view) -> bool {
    if (!view.IsUsable())
        return false;
    if (draws_.empty())
        return true; // nothing to draw is not a failure

    const algo::Vec2 origin = view.Origin();
    if (!renderer.BeginFrame(origin.GetX(), origin.GetY(), view.zoom))
        return false;

    for (const auto& draw : draws_) {
        if (draw.texture == nullptr)
            continue;
        if (draw.has_source) {
            renderer.DrawTexture(*draw.texture, draw.destination, draw.tint, &draw.source, draw.flip_x, draw.flip_y,
                                 draw.filter);
        } else {
            renderer.DrawTexture(*draw.texture, draw.destination, draw.tint, nullptr, draw.flip_x, draw.flip_y,
                                 draw.filter);
        }
    }

    return renderer.EndFrame();
}

} // namespace atom::render::scene2d
