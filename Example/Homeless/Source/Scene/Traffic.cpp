#include "Traffic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

#include <ECS/Components.hpp>

#include "PromenadeConfig.hpp"
#include "SceneAudio.hpp"
#include "SceneSprite.hpp"

namespace homeless {

namespace {

constexpr float car_width = 108.0f;
constexpr float track_lifetime = 6.0f;
constexpr std::array lane_y{261.0f, 299.0f};
constexpr std::array lane_speed{138.0f, -156.0f};

} // namespace

Traffic::Traffic(atom::ecs::EntityWorld& world, SceneAudio& audio, atom::random::Rng rng)
    : world_(world), audio_(audio), rng_(rng) {}

auto Traffic::Initialize(float world_width) -> bool {
    world_width_ = world_width;
    rng_.Shuffle(std::span{car_models_});
    for (std::size_t lane = 0; lane < traffic_.size(); ++lane) {
        const bool westbound = lane_speed[lane] < 0;
        next_car_[lane] = westbound ? 1.4f : 0.0f;
        for (auto& car : traffic_[lane]) {
            car.entity = world_.CreateEntity();
            if (!SetUpSprite(world_, car.entity, {westbound ? world_width_ + car_width : -car_width, lane_y[lane]},
                             {.texture_uri = std::string{car_uri},
                              .source_size = {car_width, 32},
                              .pivot = {0.5f, 1.0f},
                              .filter = atom::ecs::SpriteFilter::Nearest,
                              .layer = 20 + static_cast<int>(lane),
                              .flip_x = westbound,
                              .visible = false}))
                return false;
        }
    }
    return true;
}

auto Traffic::Step(float seconds, float listener_x) -> void {
    traffic_time_ += seconds;
    while (!wheel_tracks_.empty() && traffic_time_ - wheel_tracks_.front().born_at >= track_lifetime)
        wheel_tracks_.pop_front();

    for (std::size_t lane = 0; lane < traffic_.size(); ++lane) {
        const float speed = lane_speed[lane];
        const bool westbound = speed < 0;
        for (auto& car : traffic_[lane]) {
            if (!car.active)
                continue;
            auto& transform = *world_.TryGet<atom::ecs::Transform2D>(car.entity);
            const float x = transform.position.GetX() + speed * seconds;
            transform.position.SetX(x);
            if (westbound ? x < -car_width : x > world_width_ + car_width) {
                car.active = false;
                world_.TryGet<atom::ecs::SpriteRenderer>(car.entity)->visible = false;
                ++car_passes_[lane];
                continue;
            }

            const float approach_distance = westbound ? x - listener_x : listener_x - x;
            if (!car.sound_played && approach_distance <= 190.0f && approach_distance >= -32.0f)
                car.sound_played = audio_.Queue(SceneAudio::Sound::PassingCar);

            // Tracks remain on the street after the car leaves. Spacing is in world pixels.
            const float rear_axle = x + (westbound ? 29.0f : -29.0f);
            if (std::abs(rear_axle - car.last_track_x) >= 7.0f) {
                car.last_track_x = rear_axle;
                if (rear_axle >= 0 && rear_axle < world_width_)
                    wheel_tracks_.push_back({rear_axle, traffic_time_, lane});
            }
        }

        next_car_[lane] -= seconds;
        if (next_car_[lane] > 0)
            continue;
        const auto free = std::ranges::find_if(traffic_[lane], [](const Car& car) { return !car.active; });
        if (free == traffic_[lane].end())
            continue;
        const atom::algo::Vec2 start{westbound ? world_width_ + car_width : -car_width, lane_y[lane]};
        world_.TryGet<atom::ecs::Transform2D>(free->entity)->position = start;
        world_.TryGet<atom::ecs::RenderTransform2D>(free->entity)->position = start;
        auto& sprite = *world_.TryGet<atom::ecs::SpriteRenderer>(free->entity);
        sprite.visible = true;
        const auto model = NextCarModel();
        sprite.source_position = {car_width * model, 0};
        ++car_spawns_[model];
        free->last_track_x = start.GetX();
        free->active = true;
        free->sound_played = false;
        next_car_[lane] = rng_.UniformFloat(1.9f, 3.7f);
    }
}

auto Traffic::NextCarModel() -> std::size_t {
    if (car_model_cursor_ == car_models_.size()) {
        const auto previous = car_models_.back();
        rng_.Shuffle(std::span{car_models_});
        if (car_models_.front() == previous)
            std::swap(car_models_[0], car_models_[1]);
        car_model_cursor_ = 0;
    }
    return car_models_[car_model_cursor_++];
}

auto Traffic::DrawRoadDetails(atom::render::Renderer2D& renderer, float left, float right, float interpolation) const
    -> void {
    for (const auto& track : wheel_tracks_) {
        if (track.x + 7 < left || track.x > right)
            continue;
        const float age = (traffic_time_ - track.born_at) / track_lifetime;
        const auto alpha = static_cast<std::uint8_t>(66 * (1.0f - age));
        const float x = std::round(track.x);
        const float y = lane_y[track.lane] - 2;
        renderer.DrawRect({x, y, 7, 1}, {174, 187, 210, alpha});
        renderer.DrawRect({x, y - 4, 7, 1}, {147, 165, 197, alpha});
    }

    for (std::size_t lane = 0; lane < traffic_.size(); ++lane) {
        for (const auto& car : traffic_[lane]) {
            if (!car.active)
                continue;
            const auto now = world_.TryGet<atom::ecs::Transform2D>(car.entity)->position;
            const auto previous = world_.TryGet<atom::ecs::RenderTransform2D>(car.entity)->position;
            const float x = std::round(atom::algo::Vec2::Lerp(previous, now, interpolation).GetX());
            if (x + 78 < left || x - 78 > right)
                continue;
            const float light_x = lane_speed[lane] > 0 ? x + 47 : x - 72;
            renderer.DrawRect({light_x, lane_y[lane] - 4, 25, 5}, {249, 218, 153, 24});
            renderer.DrawRect({light_x + 3, lane_y[lane] - 3, 19, 2}, {255, 227, 167, 36});
        }
    }
}

auto Traffic::Summary() const -> std::string {
    std::string summary =
        "Eastbound/westbound passes: " + std::to_string(car_passes_[0]) + '/' + std::to_string(car_passes_[1]) +
        ". Active wheel tracks: " + std::to_string(wheel_tracks_.size()) + ". Spawns by vehicle model:";
    for (const auto count : car_spawns_)
        summary += ' ' + std::to_string(count);
    return summary;
}

} // namespace homeless
