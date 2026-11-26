#include "PromenadeScreen.hpp"

#include <cmath>
#include <string_view>

#include "PromenadeConfig.hpp"

namespace homeless {

namespace {

constexpr auto white = atom::color::Color::White();
constexpr auto nearest = atom::render::Filter2D::Nearest;

} // namespace

auto PromenadeScreen::SetUpWeather() -> bool {
    if (!textures_.Resolve(snow_uri) || !textures_.Resolve(breath_uri))
        return false;
    for (const bool left : {false, true}) {
        const float direction = left ? 3.38f : -0.24f;
        if (!snow_.AddPreset({.name = left ? "breath-left" : "breath-right",
                              .texture_uri = std::string(breath_uri),
                              .source = {0, 0, 7, 7},
                              .lifetime_min = 0.52f,
                              .lifetime_max = 0.86f,
                              .speed_min = 12.0f,
                              .speed_max = 20.0f,
                              .angle_min_radians = direction - 0.19f,
                              .angle_max_radians = direction + 0.19f,
                              .acceleration = {5, -8},
                              .size_start = 4,
                              .size_end = 10,
                              .tint = {225, 239, 255, 190},
                              .layer = 12,
                              .filter = atom::ecs::SpriteFilter::Nearest}))
            return false;
    }
    return snow_.AddPreset({.name = "far",
                            .texture_uri = std::string(snow_uri),
                            .source = {0, 0, 1, 1},
                            .lifetime_min = 17,
                            .lifetime_max = 22,
                            .speed_min = 22,
                            .speed_max = 29,
                            .angle_min_radians = 1.24f,
                            .angle_max_radians = 1.61f,
                            .acceleration = {0.45f, 0},
                            .size_start = 1,
                            .size_end = 1,
                            .tint = {187, 210, 242, 195},
                            .layer = 5,
                            .filter = atom::ecs::SpriteFilter::Nearest}) &&
           snow_.AddPreset({.name = "near",
                            .texture_uri = std::string(snow_uri),
                            .source = {0, 0, 1, 1},
                            .lifetime_min = 9,
                            .lifetime_max = 13,
                            .speed_min = 38,
                            .speed_max = 51,
                            .angle_min_radians = 1.04f,
                            .angle_max_radians = 1.36f,
                            .acceleration = {0.8f, 0},
                            .size_start = 2,
                            .size_end = 2,
                            .tint = {225, 236, 255, 230},
                            .layer = 30,
                            .filter = atom::ecs::SpriteFilter::Nearest});
}

auto PromenadeScreen::UpdateWeather(float seconds) -> void {
    weather_time_ = std::fmod(weather_time_ + seconds, 628.31854f);
    snow_.Advance(seconds);
    const auto emit = [&](std::string_view layer, float rate, float& remainder) {
        remainder += rate * seconds;
        while (remainder >= 1.0f) {
            const float x = snow_rng_.UniformFloat(-160, world_width_ + 12);
            (void)snow_.Burst(layer, {x, -4.0f}, 1);
            remainder -= 1.0f;
        }
    };
    emit("far", 36.0f * world_width_ / scene_width, far_snow_);
    emit("near", 12.0f * world_width_ / scene_width, near_snow_);
}

auto PromenadeScreen::DrawWater(float left, float right) -> void {
    const float gust = 1.15f + 0.7f * std::sin(weather_time_ * 0.19f);
    for (const auto& span : water_spans_) {
        if (span.x >= right || span.x + span.width <= left)
            continue;
        const float sway = std::round(gust * std::sin(span.y * 0.69f - weather_time_ * 1.8f));
        if (sway != 0) {
            auto source = span;
            source.x += sway;
            renderer_.DrawTexture(*waterfront_, span, white, &source, false, false, nearest);
        }
    }
}

} // namespace homeless
