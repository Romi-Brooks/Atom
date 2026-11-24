#include "PromenadeScreen.hpp"

#include <algorithm>
#include <cmath>
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

constexpr float walker_feet = 228.0f;
constexpr auto white = atom::color::Color::White();
constexpr auto nearest = atom::render::Filter2D::Nearest;

} // namespace

PromenadeScreen::PromenadeScreen(std::unique_ptr<atom::ecs::EntityWorld> world, unsigned frame_limit,
                                 bool& render_failed, atom::random::Rng scene_rng)
    : world_(std::move(world)), textures_(cache_, atom::fs::Vfs::GetInstance()),
      traffic_(*world_, mixer_, effects_, scene_rng), walker_rng_(scene_rng.Fork(1)), frames_remaining_(frame_limit),
      render_failed_(render_failed) {
    mixer_.SetMusicVolume(70.0f);
}

auto PromenadeScreen::SetUpWorld() -> bool {
    walker_ = world_->CreateEntity();
    camera_ = world_->CreateEntity();
    const atom::algo::Vec2 start{world_width_ * 0.5f, scene_height * 0.5f};
    if (!SetUpSprite(*world_, walker_, {start.GetX(), walker_feet},
                     {.pivot = {0.5f, 1.0f}, .filter = atom::ecs::SpriteFilter::Nearest, .layer = 10}) ||
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

    return traffic_.Initialize(world_width_) && SetUpWalkerAnimation() && SetUpWeather();
}

auto PromenadeScreen::OnActivate() -> void {
    if (ambience_.Load("waterfront", "res://Audio/Waterfront.wav")) {
        ambience_.SetLooping("waterfront", true);
        ambience_.Play("waterfront", mixer_.GetEffectiveSFXVolume() * 0.34f);
    } else {
        LOG_WARNING(atom::log::audio::Sfx, "Waterfront ambience is unavailable");
    }
    (void)effects_.SetVoiceLimit(4);
    traffic_.LoadAudio();
    LoadFootsteps();
}

auto PromenadeScreen::OnDeactivate() -> void {
    ambience_.Stop("waterfront");
    ambience_.Stop("phone");
    effects_.StopAll();
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
    return false;
}

auto PromenadeScreen::FixedUpdate(float seconds) -> void {
    (void)atom::render::scene2d::Scene2DRenderer::SnapshotTransforms(*world_);
    if (overlay_.IsMenuOpen())
        return;

    UpdateWalker(seconds);
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
    const float camera_left = std::clamp(std::round(follow.Origin().GetX()), 0.0f, world_width_ - view_width);
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
    renderer_.DrawTexture(*scene_, visible, white, &visible, false, false, nearest);
    DrawWater(camera_left, camera_left + view_width);
    traffic_.DrawRoadDetails(renderer_, camera_left, camera_left + view_width, interpolation_);

    for (const auto& draw : world_renderer_.Collect(*world_, view, nullptr, &snow_, interpolation_, true)) {
        auto dst = draw.destination;
        dst.width = std::max(1.0f, std::round(dst.width));
        dst.height = std::max(1.0f, std::round(dst.height));
        dst.x = std::round(dst.x);
        dst.y = std::round(dst.y);
        renderer_.DrawTexture(*draw.texture, dst, draw.tint, draw.has_source ? &draw.source : nullptr, draw.flip_x,
                              draw.flip_y, draw.filter);
    }
    if (!renderer_.EndFrame() || !overlay_.Draw(device)) {
        render_failed_ = true;
        atom::runtime::Application::GetInstance().RequestStop();
    } else if (frames_remaining_ != 0 && --frames_remaining_ == 0) {
        LOG_INFO(atom::log::core::Screen, "Homeless frame limit reached. Snow particles: " +
                                              std::to_string(snow_.Count()) + ". " + traffic_.Summary());
        atom::runtime::Application::GetInstance().RequestStop();
    }
}

} // namespace homeless
