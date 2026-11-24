#ifndef ATOM_HOMELESS_TRAFFIC_HPP
#define ATOM_HOMELESS_TRAFFIC_HPP

#include <array>
#include <cstddef>
#include <deque>
#include <string>

#include <ECS/EntityWorld.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/SFXPlayer.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Utilities/Random/Rng.hpp>

namespace homeless {

class Traffic final {
    public:
        Traffic(atom::ecs::EntityWorld& world, atom::AudioMixer& mixer, atom::SFXPlayer& effects,
                atom::random::Rng rng);
        auto Initialize(float world_width) -> bool;
        auto LoadAudio() -> void;
        auto Step(float seconds, float listener_x) -> void;
        auto DrawRoadDetails(atom::render::Renderer2D& renderer, float left, float right, float interpolation) const
            -> void;
        [[nodiscard]] auto Summary() const -> std::string;

    private:
        auto NextCarModel() -> std::size_t;

        struct Car {
                atom::ecs::Entity entity{};
                float last_track_x = 0.0f;
                bool active = false;
                bool sound_played = false;
        };

        struct WheelTrack {
                float x = 0.0f;
                float born_at = 0.0f;
                std::size_t lane = 0;
        };

        atom::ecs::EntityWorld& world_;
        atom::AudioMixer& mixer_;
        atom::SFXPlayer& effects_;
        atom::random::Rng rng_;
        float world_width_ = 0.0f;
        float traffic_time_ = 0.0f;
        std::array<unsigned, 2> car_passes_{};
        std::deque<WheelTrack> wheel_tracks_;
        std::array<std::array<Car, 5>, 2> traffic_{};
        std::array<float, 2> next_car_{};
        std::array<std::size_t, 4> car_models_{0, 1, 2, 3};
        std::array<unsigned, 4> car_spawns_{};
        std::size_t car_model_cursor_ = 0;
};

} // namespace homeless

#endif // ATOM_HOMELESS_TRAFFIC_HPP
