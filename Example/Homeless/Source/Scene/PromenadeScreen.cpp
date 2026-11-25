#include "PromenadeScreen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include <ECS/Components.hpp>
#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>
#include <Render/Scene2D/Scene2DRenderer.hpp>
#include <Runtime/Application.hpp>
#include <World2D/CameraSystem2D.hpp>

#include "PromenadeConfig.hpp"
#include "SceneSprite.hpp"

namespace homeless {

namespace {

constexpr auto white = atom::color::Color::White();
constexpr auto nearest = atom::render::Filter2D::Nearest;

} // namespace

PromenadeScreen::PromenadeScreen(std::unique_ptr<atom::ecs::EntityWorld> world, unsigned frame_limit,
                                 bool& render_failed, atom::random::Rng scene_rng)
    : world_(std::move(world)), textures_(cache_, atom::fs::Vfs::GetInstance()),
      traffic_(*world_, scene_audio_, scene_rng), pedestrians_(*world_, scene_rng.Fork(2)),
      walker_rng_(scene_rng.Fork(1)), frames_remaining_(frame_limit), render_failed_(render_failed) {
    mixer_.SetMusicVolume(70.0f);
}

auto PromenadeScreen::SetUpWorld() -> bool {
    walker_ = world_->CreateEntity();
    camera_ = world_->CreateEntity();
    const atom::algo::Vec2 start{world_width_ * 0.5f, scene_height * 0.5f};
    if (!SetUpSprite(*world_, walker_, {start.GetX(), walker_feet},
                     {.pivot = {0.5f, 1.0f}, .filter = atom::ecs::SpriteFilter::Nearest, .layer = walker_layer}) ||
        !world_->Emplace<atom::ecs::Animator2D>(walker_) ||
        !world_->Emplace<atom::ecs::SceneEntityId>(walker_, atom::ecs::SceneEntityId{.value = 1}) ||
        !world_->Emplace<atom::ecs::Transform2D>(camera_, atom::ecs::Transform2D{.position = start}) ||
        !world_->Emplace<atom::ecs::RenderTransform2D>(camera_, atom::ecs::RenderTransform2D{.position = start}) ||
        !world_->Emplace<atom::ecs::Camera2D>(camera_,
                                              atom::ecs::Camera2D{.viewport_size = {scene_width, scene_height}}) ||
        !world_->Emplace<atom::ecs::CameraRig2D>(camera_,
                                                 atom::ecs::CameraRig2D{.target_scene_id = 1,
                                                                        .follow_half_life = 0.16f,
                                                                        .dead_zone = {52, scene_height},
                                                                        .clamp_to_bounds = true,
                                                                        .bounds_max = {world_width_, scene_height}}))
        return false;

    if (!traffic_.Initialize(world_width_) || !SetUpWalkerAnimation() || !SetUpWeather() ||
        !pedestrians_.Initialize(world_width_, clips_))
        return false;
    pedestrians_.Reset(false, animation_);
    StepAnimation(0.0f);
    return true;
}

auto PromenadeScreen::OnActivate() -> void {
    scene_audio_.Start();
}

auto PromenadeScreen::OnDeactivate() -> void {
    scene_audio_.Stop();
    overlay_.StopMusic();
}

auto PromenadeScreen::HandleEvent(const atom::window::IEvent& event) -> bool {
    if (overlay_.HandleEvent(event))
        return true;
    const auto* key = std::get_if<atom::window::KeyEvent>(&event.data);
    if (event.type != atom::window::EventType::KeyPressed || !key || key->is_repeat)
        return false;
    if (key->key == atom::input::Key::M) {
        overlay_.ToggleEffects();
        return true;
    }
    if (key->key == atom::input::Key::E && !overlay_.IsMenuOpen() && NearbyEntrance()) {
        entrance_checked_ = true;
        return true;
    }
    return false;
}

auto PromenadeScreen::FixedUpdate(float seconds) -> void {
    (void)atom::render::scene2d::Scene2DRenderer::SnapshotTransforms(*world_);
    if (overlay_.IsMenuOpen())
        return;

    if (fade_ == Fade::None) {
        UpdateWalker(seconds);
        if (entrance_checked_ && !NearbyEntrance())
            entrance_checked_ = false;
    } else {
        UpdateAreaTransition(seconds);
    }
    pedestrians_.Step(seconds, animation_);
    StepAnimation(seconds);
    UpdateWeather(seconds);
    StepTraffic(seconds);
    (void)atom::world2d::CameraSystem2D::Step(*world_, {scene_width, scene_height}, seconds);
}

auto PromenadeScreen::StepTraffic(float seconds) -> void {
    const float listener_x = world_->TryGet<atom::ecs::Transform2D>(walker_)->position.GetX();
    traffic_.Step(seconds, listener_x);
}

auto PromenadeScreen::Render(atom::render::IRenderDevice& device) -> void {
    const auto output = device.GetOutputSize();
    if (output.GetX() <= 0 || output.GetY() <= 0)
        return;
    // Fit to street height at ordinary aspect ratios and reveal more of the
    // panorama as the window widens. On ultrawide outputs, trim sky first.
    const float scale = std::max(output.GetY() / scene_height, output.GetX() / world_width_);
    const float view_width = std::min(world_width_, output.GetX() / scale);
    const float view_height = std::min(static_cast<float>(scene_height), output.GetY() / scale);
    world_->TryGet<atom::ecs::Camera2D>(camera_)->viewport_size = {view_width, view_height};
    const auto follow =
        atom::render::scene2d::Scene2DRenderer::FindActiveCamera(*world_, {view_width, view_height}, interpolation_);
    const float camera_left =
        std::clamp(std::round(follow.Origin().GetX() * scale) / scale, 0.0f, world_width_ - view_width);
    const float camera_top = scene_height - view_height;
    const atom::render::scene2d::CameraView view{
        .center = {camera_left + view_width * 0.5f, camera_top + view_height * 0.5f},
        .viewport = {view_width, view_height}};
    if (!renderer_.BeginFrame(camera_left, camera_top, scale)) {
        render_failed_ = true;
        atom::runtime::Application::GetInstance().RequestStop();
        return;
    }
    const atom::algo::Rect visible{camera_left, camera_top, view_width, view_height};
    auto& background = area_ == Area::Street ? *street_ : *waterfront_;
    renderer_.DrawTexture(background, visible, white, &visible, false, false, nearest);
    if (area_ == Area::Waterfront)
        DrawWater(camera_left, camera_left + view_width);
    traffic_.DrawRoadDetails(renderer_, camera_left, camera_left + view_width, interpolation_);

    for (const auto& draw : world_renderer_.Collect(*world_, view, nullptr, &snow_, interpolation_, true)) {
        auto dst = draw.destination;
        dst.width = std::max(1.0f, std::round(dst.width));
        dst.height = std::max(1.0f, std::round(dst.height));
        // Let interpolated characters move by one output pixel at enlarged scales.
        if (draw.layer == walker_layer || draw.layer == pedestrian_layer)
            dst.x = camera_left + std::round((dst.x - camera_left) * scale) / scale;
        else
            dst.x = std::round(dst.x);
        dst.y = std::round(dst.y);
        renderer_.DrawTexture(*draw.texture, dst, draw.tint, draw.has_source ? &draw.source : nullptr, draw.flip_x,
                              draw.flip_y, draw.filter);
    }
    if (fade_opacity_ > 0.0f)
        renderer_.DrawRect(visible, {12, 9, 22, static_cast<std::uint8_t>(std::round(fade_opacity_ * 255.0f))});
    InteractionPrompt prompt;
    if (const auto* entrance = NearbyEntrance())
        prompt = {entrance->name, entrance->fallback_name, entrance_checked_};
    if (!renderer_.EndFrame() || !overlay_.Draw(device, prompt)) {
        render_failed_ = true;
        atom::runtime::Application::GetInstance().RequestStop();
    } else if (frames_remaining_ != 0 && --frames_remaining_ == 0) {
        LOG_INFO(atom::log::core::Screen,
                 "Homeless frame limit reached. Snow particles: " + std::to_string(snow_.Count()) +
                     ". Pedestrians: " + std::to_string(pedestrians_.Count()) + ". " + traffic_.Summary());
        atom::runtime::Application::GetInstance().RequestStop();
    }
}

} // namespace homeless
