#include "PromenadeScreen.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <ECS/Components.hpp>

#include "PromenadeConfig.hpp"

namespace homeless {

namespace {

constexpr float fade_duration = 0.18f;
constexpr float arrival_margin = 42.0f;
constexpr float entrance_radius = 28.0f;
constexpr std::array entrances{
    StreetEntrance{278.0f, "小吃摊", "Food stall"},
    StreetEntrance{492.0f, "小卖部", "Corner shop"},
    StreetEntrance{734.0f, "网吧", "Internet cafe"},
};

} // namespace

auto PromenadeScreen::CheckAreaExit(int direction, float x) -> bool {
    const bool at_exit = area_ == Area::Waterfront ? direction > 0 && x >= world_width_ - walk_boundary
                                                   : direction < 0 && x <= walk_boundary;
    if (at_exit) {
        fade_ = Fade::Out;
        entrance_checked_ = false;
    }
    return at_exit;
}

auto PromenadeScreen::UpdateAreaTransition(float seconds) -> void {
    const float step = seconds / fade_duration;
    if (fade_ == Fade::Out) {
        fade_opacity_ = std::min(1.0f, fade_opacity_ + step);
        if (fade_opacity_ >= 1.0f) {
            SwitchArea();
            fade_ = Fade::In;
        }
    } else {
        fade_opacity_ = std::max(0.0f, fade_opacity_ - step);
        if (fade_opacity_ <= 0.0f)
            fade_ = Fade::None;
    }
}

auto PromenadeScreen::SwitchArea() -> void {
    area_ = area_ == Area::Waterfront ? Area::Street : Area::Waterfront;
    const float x = area_ == Area::Street ? arrival_margin : world_width_ - arrival_margin;
    const atom::algo::Vec2 position{x, walker_feet};
    world_->TryGet<atom::ecs::Transform2D>(walker_)->position = position;
    world_->TryGet<atom::ecs::RenderTransform2D>(walker_)->position = position;
    walk_velocity_ = 0.0f;
    (void)animation_.SetState(*world_, walker_, "idle");
    world_->TryGet<atom::ecs::Animator2D>(walker_)->speed = 1.0f;
    pedestrians_.Reset(area_ == Area::Street, animation_);

    const float half_view = world_->TryGet<atom::ecs::Camera2D>(camera_)->viewport_size.GetX() * 0.5f;
    const atom::algo::Vec2 center{std::clamp(x, half_view, world_width_ - half_view), scene_height * 0.5f};
    auto& rig = *world_->TryGet<atom::ecs::CameraRig2D>(camera_);
    rig.base_position = center;
    rig.initialized = true;
    world_->TryGet<atom::ecs::Transform2D>(camera_)->position = center;
    world_->TryGet<atom::ecs::RenderTransform2D>(camera_)->position = center;
}

auto PromenadeScreen::NearbyEntrance() const -> const StreetEntrance* {
    if (area_ != Area::Street || fade_ != Fade::None)
        return nullptr;
    const float x = world_->TryGet<atom::ecs::Transform2D>(walker_)->position.GetX();
    const auto entrance = std::ranges::find_if(
        entrances, [x](const StreetEntrance& entry) { return std::abs(entry.x - x) <= entrance_radius; });
    return entrance == entrances.end() ? nullptr : &*entrance;
}

} // namespace homeless
