/**
  * @file           : Entity.hpp
  * @author         : Romi Brooks
  * @brief          : Entity base class using engine interfaces
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_ENTITY_HPP
#define ATOM_ENTITY_HPP

#include <memory>

#include <Algorithm/Vector/Vec2.hpp>
#include <Config/Movement/MoveEvent.hpp>
#include <Backend/Contracts/Render/IRenderTarget.hpp>
#include <Backend/Contracts/Render/ITexture.hpp>

namespace atom {
    class Entity {
        protected:
            std::unique_ptr<ITexture> texture_;
            float radius_ = 0;
            Color color_{Color::White()};
            mutable Vec2 position_; // mutable so const Move() can update it

            float hp_;
            float attack_;
            float move_speed_ {};
            float move_acceleration_ {};

        public:
            Entity(float hp, float attack, float moveSpeed, float moveAcceleration)
            : hp_(hp), attack_(attack), move_speed_(moveSpeed), move_acceleration_(moveAcceleration) {}

            virtual ~Entity() = default;

            Entity(Entity&& other) noexcept
            : texture_(std::move(other.texture_)),
              radius_(other.radius_), color_(other.color_), position_(other.position_),
              hp_(other.hp_), attack_(other.attack_),
              move_speed_(other.move_speed_), move_acceleration_(other.move_acceleration_) {}

            Entity& operator=(Entity&& other) noexcept {
                if (this != &other) {
                    texture_ = std::move(other.texture_);
                    radius_ = other.radius_;
                    color_ = other.color_;
                    position_ = other.position_;
                    hp_ = other.hp_;
                    attack_ = other.attack_;
                    move_speed_ = other.move_speed_;
                    move_acceleration_ = other.move_acceleration_;
                }
                return *this;
            }

            auto CreateCircleWithColor(float radius, const Color& color) -> void {
                radius_ = radius;
                color_ = color;
            }

            auto CreateCircle(float radius) -> void {
                radius_ = radius;
                color_ = Color::White();
            }

            [[nodiscard]] auto LoadTexture(const std::string& path) -> bool {
                if (!texture_) return false;
                return texture_->LoadFromFile(path);
            }

            virtual auto Move(const Movement Signal) const -> void {}

            auto Attack(Entity& target) const -> bool {
                if (!IsAlive()) return false;
                target.Damage(attack_);
                return true;
            }

            auto Damage(float damage) -> bool {
                if (!IsAlive()) return false;
                hp_ -= damage;
                return true;
            }

            [[nodiscard]] auto IsAlive() const -> bool {
                return hp_ > 0;
            }

            auto SetPosition(float x, float y) -> void {
                position_ = {x, y};
            }

            auto SetBloody(float bloody) -> void {
                hp_ = bloody;
            }

            auto SetAttack(float attack) -> void {
                attack_ = attack;
            }

            auto SetMoveSpeed(float moveSpeed) -> void {
                move_speed_ = moveSpeed;
            }

            auto SetMoveAcceleration(float moveAcceleration) -> void {
                move_acceleration_ = moveAcceleration;
            }

            [[nodiscard]] auto GetRadius() const -> float {
                return radius_;
            }

            [[nodiscard]] auto GetHP() const -> float {
                return hp_;
            }

            [[nodiscard]] auto GetAttack() const -> float {
                return attack_;
            }

            [[nodiscard]] auto GetPosition() const -> Vec2 {
                return position_;
            }

            virtual auto Draw(IRenderTarget& target) const -> void {
                if (texture_) {
                    target.DrawTexture(*texture_, position_.GetX(), position_.GetY());
                } else if (radius_ > 0) {
                    target.DrawCircle(position_.GetX(), position_.GetY(), radius_, color_);
                }
            }

            Entity(const Entity&) = delete;
            Entity& operator=(const Entity&) = delete;
    };
}

#endif // ATOM_ENTITY_HPP
