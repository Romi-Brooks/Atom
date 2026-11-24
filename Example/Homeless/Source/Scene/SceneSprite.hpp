#ifndef ATOM_HOMELESS_SCENESPRITE_HPP
#define ATOM_HOMELESS_SCENESPRITE_HPP

#include <utility>

#include <ECS/Components.hpp>
#include <ECS/EntityWorld.hpp>

namespace homeless {

inline auto SetUpSprite(atom::ecs::EntityWorld& world, atom::ecs::Entity entity, atom::algo::Vec2 position,
                        atom::ecs::SpriteRenderer sprite) -> bool {
    return world.Emplace<atom::ecs::Transform2D>(entity, atom::ecs::Transform2D{.position = position}) &&
           world.Emplace<atom::ecs::RenderTransform2D>(entity, atom::ecs::RenderTransform2D{.position = position}) &&
           world.Emplace<atom::ecs::SpriteRenderer>(entity, std::move(sprite));
}

} // namespace homeless

#endif // ATOM_HOMELESS_SCENESPRITE_HPP
