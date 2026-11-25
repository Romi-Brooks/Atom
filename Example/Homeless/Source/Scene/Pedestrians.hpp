#ifndef ATOM_HOMELESS_PEDESTRIANS_HPP
#define ATOM_HOMELESS_PEDESTRIANS_HPP

#include <array>
#include <cstddef>
#include <string>

#include <ECS/EntityWorld.hpp>
#include <Utilities/Random/Rng.hpp>
#include <World2D/Animation2D.hpp>

namespace homeless {

class Pedestrians final {
    public:
        static constexpr int frame_width = 32;
        static constexpr int frame_height = 46;
        static constexpr int frame_count = 4;
        static constexpr int outfit_count = 4;
        static constexpr int color_count = 3;

        Pedestrians(atom::ecs::EntityWorld& world, atom::random::Rng rng) : world_(world), rng_(rng) {}
        auto Initialize(float width, atom::world2d::AnimationLibrary2D& clips) -> bool;
        auto Reset(bool street, const atom::world2d::AnimationSystem2D& animation) -> void;
        auto Step(float seconds, const atom::world2d::AnimationSystem2D& animation) -> void;
        [[nodiscard]] auto Count() const -> std::size_t;

    private:
        struct Group {
                std::array<atom::ecs::Entity, 2> members{};
                float x = 0;
                float feet = 0;
                float velocity = 0;
                float spacing = 0;
                std::size_t count = 0;
        };

        auto Spawn(Group& group, float x, int direction, const atom::world2d::AnimationSystem2D& animation) -> void;
        auto Place(const Group& group, bool snap) -> void;
        auto Hide(Group& group) -> void;

        atom::ecs::EntityWorld& world_;
        atom::random::Rng rng_;
        std::array<Group, 4> groups_{};
        std::array<std::string, outfit_count * color_count> states_{};
        float width_ = 0;
        float next_spawn_ = 0;
        bool street_ = false;
};

} // namespace homeless

#endif // ATOM_HOMELESS_PEDESTRIANS_HPP
