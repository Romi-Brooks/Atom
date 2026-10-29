/**
 * @file           : Scene2DRendererTest.cpp
 * @brief          : T1.2 - sprite collection, sorting, camera mapping, pivots.
 *
 * Headless on purpose (T4.1's first instalment): every decision the render pass
 * makes - which sprites are drawn, in what order, with what destination rect and
 * under which camera - is produced by Collect()/FindActiveCamera(), which touch
 * no GPU. Submit() is the only part that needs a renderer, and it makes no
 * decisions of its own, so there is nothing left to get wrong there that this
 * test could not already assert.
 *
 * The stub resolver hands out distinct Renderer2D::Texture addresses without a
 * device. Their GetWidth()/GetHeight() are 0, which is exactly the shape a
 * texture whose dimensions are unknown has, so the tests always state an explicit
 * sprite size - the "fall back to the texture's own size" path is one line and is
 * covered by the unit that computes it.
 */

#include <Render/Scene2D/Scene2DRenderer.hpp>

#include <ECS/Components.hpp>
#include <ECS/EntityWorld.hpp>

#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using namespace atom;
using namespace atom::render::scene2d;
using atom::render::Renderer2D;

// Maps URI -> a stable stub texture; unknown URIs resolve to nullptr, which is
// how a missing asset behaves.
class StubResolver final : public ITextureResolver {
    public:
        auto Add(const std::string& uri) -> Renderer2D::Texture* {
            auto& texture = textures_[uri];
            return &texture;
        }

        auto Resolve(const std::string_view uri) -> Renderer2D::Texture* override {
            ++calls;
            const auto found = textures_.find(std::string{uri});
            return found == textures_.end() ? nullptr : &found->second;
        }

        std::size_t calls = 0;

    private:
        std::unordered_map<std::string, Renderer2D::Texture> textures_{};
};

auto MakeSprite(const std::string& uri, const float size_x, const float size_y) -> ecs::SpriteRenderer {
    ecs::SpriteRenderer sprite{};
    sprite.texture_uri = uri;
    sprite.size = {size_x, size_y};
    return sprite;
}

} // namespace

auto main() -> int {
    // --- 1. CameraView: origin is the only camera conversion --------------
    {
        CameraView view{};
        view.center = {100.0f, 50.0f};
        view.viewport = {800.0f, 600.0f};
        view.zoom = 2.0f;
        ATOM_CHECK(view.IsUsable());

        // origin = center - viewport / (2 * zoom)
        const auto origin = view.Origin();
        ATOM_CHECK_NEAR(origin.GetX(), 100.0f - 400.0f / 2.0f);
        ATOM_CHECK_NEAR(origin.GetY(), 50.0f - 300.0f / 2.0f);

        // Zoom 1 with a 2:1 viewport centres on the same point.
        view.zoom = 1.0f;
        const auto origin_1x = view.Origin();
        ATOM_CHECK_NEAR(origin_1x.GetX(), -300.0f);
        ATOM_CHECK_NEAR(origin_1x.GetY(), -250.0f);

        // A degenerate lens is rejected instead of inverting the scene.
        view.zoom = 0.0f;
        ATOM_CHECK(!view.IsUsable());
        view.zoom = -1.0f;
        ATOM_CHECK(!view.IsUsable());
        view.zoom = 1.0f;
        view.center = {};
        ATOM_CHECK(view.IsUsable());
    }

    auto created = ecs::EntityWorld::Create();
    ATOM_CHECK(created.has_value());
    auto& world = **created;

    StubResolver resolver{};
    auto* player_texture = resolver.Add("res://textures/player.png");
    auto* crate_texture = resolver.Add("res://textures/crate.png");
    auto* floor_texture = resolver.Add("res://textures/floor.png");
    ATOM_CHECK(player_texture != nullptr && crate_texture != nullptr && floor_texture != nullptr);

    // --- 2. SnapshotTransforms copies simulation into the display state ---
    //
    // The display copy is what the render pass reads, so a world whose display
    // state was never snapshotted must draw the old (or zero) values, never the
    // simulated ones.
    {
        const auto entity = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(entity, ecs::Transform2D{.position = {10.0f, 20.0f},
                                                                            .scale = {2.0f, 2.0f}})
                        .has_value());
        ATOM_CHECK(world.Emplace<ecs::RenderTransform2D>(entity).has_value());
        ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(entity, MakeSprite("res://textures/player.png", 8.0f, 8.0f))
                        .has_value());

        const auto touched = Scene2DRenderer::SnapshotTransforms(world);
        ATOM_CHECK(touched == 1);

        const auto* display = world.TryGet<ecs::RenderTransform2D>(entity);
        ATOM_CHECK(display != nullptr);
        ATOM_CHECK_NEAR(display->position.GetX(), 10.0f);
        ATOM_CHECK_NEAR(display->scale.GetY(), 2.0f);

        // Moving the simulation transform must not move the sprite until the next
        // snapshot: the renderer never reads Transform2D.
        world.TryGet<ecs::Transform2D>(entity)->position = {999.0f, 999.0f};
        Scene2DRenderer renderer{resolver};
        const auto& before = renderer.Collect(world);
        ATOM_CHECK(before.size() == 1);
        ATOM_CHECK_NEAR(before[0].destination.x, 10.0f - 0.5f * 16.0f);
        ATOM_CHECK_NEAR(before[0].destination.width, 16.0f);

        // ... and the snapshot picks it up.
        ATOM_CHECK(Scene2DRenderer::SnapshotTransforms(world) == 1);
        const auto& after = renderer.Collect(world);
        ATOM_CHECK(after.size() == 1);
        ATOM_CHECK_NEAR(after[0].destination.x, 999.0f - 0.5f * 16.0f);

        ATOM_CHECK(world.Destroy(entity).has_value());
    }

    // --- 3. Destination rect: pivot, size, scale --------------------------
    {
        const auto entity = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(entity, ecs::Transform2D{.position = {100.0f, 100.0f},
                                                                            .scale = {2.0f, 3.0f}})
                        .has_value());
        ATOM_CHECK(world.Emplace<ecs::RenderTransform2D>(entity).has_value());

        auto sprite = MakeSprite("res://textures/player.png", 32.0f, 64.0f);
        sprite.pivot = {0.5f, 0.5f};
        ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(entity, sprite).has_value());
        ATOM_CHECK(Scene2DRenderer::SnapshotTransforms(world) == 1);

        Scene2DRenderer renderer{resolver};
        {
            const auto& draws = renderer.Collect(world);
            ATOM_CHECK(draws.size() == 1);
            // 32*2 = 64 wide, 64*3 = 192 high, centred on the pivot.
            ATOM_CHECK_NEAR(draws[0].destination.width, 64.0f);
            ATOM_CHECK_NEAR(draws[0].destination.height, 192.0f);
            ATOM_CHECK_NEAR(draws[0].destination.x, 100.0f - 32.0f);
            ATOM_CHECK_NEAR(draws[0].destination.y, 100.0f - 96.0f);
        }

        // Bottom-left pivot puts the entity position at the sprite's bottom-left.
        world.TryGet<ecs::SpriteRenderer>(entity)->pivot = {0.0f, 1.0f};
        {
            const auto& draws = renderer.Collect(world);
            ATOM_CHECK(draws.size() == 1);
            ATOM_CHECK_NEAR(draws[0].destination.x, 100.0f);
            ATOM_CHECK_NEAR(draws[0].destination.y, 100.0f - 192.0f);
        }

        // No explicit size: the atlas sub-rect doubles as the pixel size (1 texel
        // = 1 px at scale 1), and it also becomes the source rect.
        {
            auto& stored = *world.TryGet<ecs::SpriteRenderer>(entity);
            stored.size = {};
            stored.source_position = {16.0f, 32.0f};
            stored.source_size = {24.0f, 8.0f};
            stored.pivot = {0.5f, 0.5f};

            const auto& draws = renderer.Collect(world);
            ATOM_CHECK(draws.size() == 1);
            ATOM_CHECK(draws[0].has_source);
            ATOM_CHECK_NEAR(draws[0].source.x, 16.0f);
            ATOM_CHECK_NEAR(draws[0].source.y, 32.0f);
            ATOM_CHECK_NEAR(draws[0].source.width, 24.0f);
            ATOM_CHECK_NEAR(draws[0].source.height, 8.0f);
            ATOM_CHECK_NEAR(draws[0].destination.width, 24.0f * 2.0f);
            ATOM_CHECK_NEAR(draws[0].destination.height, 8.0f * 3.0f);
        }

        // A zero size everywhere is degenerate: skipped and counted, never emitted
        // as a NaN/degenerate quad.
        {
            auto& stored = *world.TryGet<ecs::SpriteRenderer>(entity);
            stored.size = {};
            stored.source_size = {};
            const auto& draws = renderer.Collect(world);
            ATOM_CHECK(draws.empty());
            ATOM_CHECK(renderer.Stats().degenerate == 1);
        }

        ATOM_CHECK(world.Destroy(entity).has_value());
    }

    // --- 4. Sorting: layer, then sort_key, then entity index --------------
    {
        // Created deliberately out of order, with a deliberate key collision so the
        // entity-index tie-break is exercised.
        const auto high_layer = world.CreateEntity();
        const auto same_layer_b = world.CreateEntity();
        const auto same_layer_a = world.CreateEntity();
        const auto low_layer = world.CreateEntity();

        auto place = [&world](const ecs::Entity entity, const std::string& uri, const std::int32_t layer,
                              const std::int32_t sort_key) -> bool {
            if (!world.Emplace<ecs::Transform2D>(entity).has_value())
                return false;
            if (!world.Emplace<ecs::RenderTransform2D>(entity).has_value())
                return false;
            auto sprite = MakeSprite(uri, 4.0f, 4.0f);
            sprite.layer = layer;
            sprite.sort_key = sort_key;
            return world.Emplace<ecs::SpriteRenderer>(entity, sprite).has_value();
        };

        ATOM_CHECK(place(high_layer, "res://textures/player.png", 10, 0));
        ATOM_CHECK(place(same_layer_b, "res://textures/crate.png", 0, 5));
        ATOM_CHECK(place(same_layer_a, "res://textures/floor.png", 0, 5));
        ATOM_CHECK(place(low_layer, "res://textures/floor.png", -3, 99));

        ATOM_CHECK(Scene2DRenderer::SnapshotTransforms(world) == 4);

        Scene2DRenderer renderer{resolver};
        const auto& draws = renderer.Collect(world);
        ATOM_CHECK(draws.size() == 4);

        // layer -3 first, then the two layer-0 items in entity order, then layer 10.
        ATOM_CHECK(draws[0].entity == low_layer);
        ATOM_CHECK(draws[1].entity.Index() < draws[2].entity.Index());
        ATOM_CHECK(draws[1].layer == 0 && draws[2].layer == 0);
        ATOM_CHECK(draws[1].sort_key == 5 && draws[2].sort_key == 5);
        ATOM_CHECK(draws[3].entity == high_layer);

        // Stable across repeated collects: the same world must not reorder.
        std::vector<ecs::Entity> before{};
        for (const auto& draw : draws)
            before.push_back(draw.entity);

        const auto& again = renderer.Collect(world);
        ATOM_CHECK(again.size() == before.size());
        for (std::size_t i = 0; i < before.size(); ++i)
            ATOM_CHECK(again[i].entity == before[i]);

        // layer/sort_key are carried through for diagnostics.
        ATOM_CHECK(draws[0].layer == -3);
        ATOM_CHECK(draws[0].sort_key == 99);
        ATOM_CHECK(draws[3].layer == 10);
    }

    // --- 5. Missing and unresolved textures are skipped, not substituted --
    {
        const auto missing = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(missing).has_value());
        ATOM_CHECK(world.Emplace<ecs::RenderTransform2D>(missing).has_value());
        ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(missing, MakeSprite("res://textures/nope.png", 4.0f, 4.0f))
                        .has_value());

        const auto no_uri = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(no_uri).has_value());
        ATOM_CHECK(world.Emplace<ecs::RenderTransform2D>(no_uri).has_value());
        ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(no_uri, MakeSprite("", 4.0f, 4.0f)).has_value());

        ATOM_CHECK(Scene2DRenderer::SnapshotTransforms(world) == 6);

        Scene2DRenderer renderer{resolver};
        const auto& draws = renderer.Collect(world);
        ATOM_CHECK(draws.size() == 4); // the four real sprites
        ATOM_CHECK(renderer.Stats().considered == 6);
        ATOM_CHECK(renderer.Stats().drawn == 4);
        ATOM_CHECK(renderer.Stats().unresolved == 2);
        ATOM_CHECK(renderer.LastUnresolvedUri() == "res://textures/nope.png");

        // A sprite without a display transform is invisible rather than drawn from
        // simulated state.
        const auto no_display = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(no_display).has_value());
        ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(no_display, MakeSprite("res://textures/crate.png", 4.0f, 4.0f))
                        .has_value());
        const auto& still = renderer.Collect(world);
        ATOM_CHECK(still.size() == 4);
        ATOM_CHECK(renderer.Stats().considered == 6); // still not in the sprite set
    }

    // --- 6. FindActiveCamera: lowest index wins, no camera is survivable --
    {
        CameraView none = Scene2DRenderer::FindActiveCamera(world, {800.0f, 600.0f});
        ATOM_CHECK(none.IsUsable());
        ATOM_CHECK_NEAR(none.zoom, 1.0f);
        ATOM_CHECK_NEAR(none.viewport.GetX(), 800.0f);
        ATOM_CHECK_NEAR(none.center.GetX(), 0.0f);

        const auto camera = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(camera, ecs::Transform2D{.position = {64.0f, 48.0f}}).has_value());
        ATOM_CHECK(world.Emplace<ecs::Camera2D>(camera, ecs::Camera2D{.zoom = 3.0f}).has_value());

        const auto view = Scene2DRenderer::FindActiveCamera(world, {800.0f, 600.0f});
        ATOM_CHECK_NEAR(view.zoom, 3.0f);
        ATOM_CHECK_NEAR(view.center.GetX(), 64.0f);
        ATOM_CHECK_NEAR(view.center.GetY(), 48.0f);
        // No viewport on the component: the caller's fallback is kept.
        ATOM_CHECK_NEAR(view.viewport.GetX(), 800.0f);

        // A second, later camera must not take over.
        const auto second_camera = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(second_camera, ecs::Transform2D{.position = {-5.0f, -5.0f}})
                        .has_value());
        ATOM_CHECK(world.Emplace<ecs::Camera2D>(second_camera, ecs::Camera2D{.zoom = 9.0f}).has_value());
        const auto still_first = Scene2DRenderer::FindActiveCamera(world, {});
        ATOM_CHECK_NEAR(still_first.zoom, 3.0f);
        ATOM_CHECK_NEAR(still_first.center.GetX(), 64.0f);

        // An explicit viewport on the component wins over the fallback, and a
        // non-positive zoom degrades to 1 instead of blanking the screen.
        ATOM_CHECK(world.Remove<ecs::Camera2D>(camera).has_value());
        ATOM_CHECK(world.Emplace<ecs::Camera2D>(camera, ecs::Camera2D{.zoom = 0.0f, .viewport_size = {320.0f, 240.0f}})
                        .has_value());
        const auto repaired = Scene2DRenderer::FindActiveCamera(world, {800.0f, 600.0f});
        ATOM_CHECK_NEAR(repaired.zoom, 1.0f);
        ATOM_CHECK_NEAR(repaired.viewport.GetX(), 320.0f);
        ATOM_CHECK_NEAR(repaired.viewport.GetY(), 240.0f);
    }

    // --- 6b. The sprite filter reaches the draw item ----------------------
    //
    // This is the only place a pixel-art sheet's sampling choice can be lost: the
    // component carries it, the draw item must, and Submit hands it to DrawTexture.
    {
        const auto crisp = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(crisp).has_value());
        ATOM_CHECK(world.Emplace<ecs::RenderTransform2D>(crisp).has_value());
        auto sprite = MakeSprite("res://textures/player.png", 4.0F, 4.0F);
        sprite.filter = ecs::SpriteFilter::Nearest;
        ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(crisp, sprite).has_value());
        ATOM_CHECK(Scene2DRenderer::SnapshotTransforms(world) >= 1);

        Scene2DRenderer renderer{resolver};
        const auto& draws = renderer.Collect(world);
        bool saw_nearest = false;
        bool saw_linear = false;
        for (const auto& draw : draws) {
            if (draw.entity != crisp)
                saw_linear = true;
            else if (draw.filter == render::Filter2D::Nearest)
                saw_nearest = true;
        }
        ATOM_CHECK(saw_nearest);
        ATOM_CHECK(saw_linear); // every other sprite still defaults to linear
    }

    // --- 7. Flip flags are carried through to the draw item ---------------
    {
        const auto flipped = world.CreateEntity();
        ATOM_CHECK(world.Emplace<ecs::Transform2D>(flipped).has_value());
        ATOM_CHECK(world.Emplace<ecs::RenderTransform2D>(flipped).has_value());
        auto sprite = MakeSprite("res://textures/player.png", 4.0f, 4.0f);
        sprite.flip_x = true;
        ATOM_CHECK(world.Emplace<ecs::SpriteRenderer>(flipped, sprite).has_value());
        ATOM_CHECK(Scene2DRenderer::SnapshotTransforms(world) >= 1);

        Scene2DRenderer renderer{resolver};
        const auto& draws = renderer.Collect(world);
        bool saw_flip = false;
        for (const auto& draw : draws) {
            if (draw.entity != flipped)
                continue;
            saw_flip = draw.flip_x && !draw.flip_y;
        }
        ATOM_CHECK(saw_flip);
    }

    return 0;
}
