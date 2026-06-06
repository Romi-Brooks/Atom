/**
  * @file           : Entity.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/


#ifndef ATOM_ENTITY_HPP
#define ATOM_ENTITY_HPP

// Standard Library
#include <memory>

// Third party Library
#include <SFML/Graphics.hpp>

// Engine Headers
#include <Config/Movement/MoveEvent.hpp>

namespace atom {
	class Entity {
		private:

		protected:
			std::unique_ptr<sf::Texture> texture_;
			std::unique_ptr<sf::CircleShape> shape_;

			float hp_;
			float attack_;
			float move_speed_ {};
			float move_acceleration_ {};

		public:
			Entity(const float hp, const float attack, const float moveSpeed, const float moveAcceleration)
			: hp_(hp), attack_(attack), move_speed_(moveSpeed), move_acceleration_(moveAcceleration) {};

			virtual ~Entity() = default;

			Entity(Entity&& other) noexcept
			: shape_(std::move(other.shape_)), hp_(other.hp_), attack_(other.attack_),
			move_speed_(other.move_speed_), move_acceleration_(other.move_acceleration_) {}

			// Move assignment operator
			// 移动赋值运算符
			Entity& operator=(Entity&& other) noexcept {
				if (this != &other) {
					shape_ = std::move(other.shape_);
				}
				return *this;
			}

			auto CreateCircleWithColor(float radius, const sf::Color& color) -> sf::Shape* {
					shape_ = std::make_unique<sf::CircleShape>(radius);
					shape_->setFillColor(color);
					return shape_.get();
				}

			auto CreateCircle(float radius) -> sf::Shape* {
					shape_ = std::make_unique<sf::CircleShape>(radius);
					return shape_.get();
				}

			[[nodiscard]] auto LoadTexture(const std::string& path) const -> bool {
				if (this->texture_->loadFromFile(path) == false) {
					return false;
				}

				if (!this->shape_) {
					return false;
				}

				this->shape_->setTexture(this->texture_.get());
				this->shape_->setTextureRect(sf::IntRect());
				return true;
			}

			virtual auto Move(const Movement Signal) const -> void {
					// should give to subclass
			}

			auto Attack(Entity& target) const -> bool {
				if (!this->IsAlive()) {
					return false;
				}
				target.Damage(this->attack_);
				return true;
			}

			auto Damage(const float damage) -> bool {
				if (!this->IsAlive()) {
					return false;
				}
				this->hp_= this->hp_ - damage;
				return true;
			}

			[[nodiscard]] auto IsAlive() const -> bool {
					return this->hp_ > 0;
				}

			auto SetPosition(float x, float y) const -> void {
				shape_->setPosition({x, y});
			}

			auto SetBloody(const float bloody) -> void {
				this->hp_ = bloody;
			}

			auto SetAttack(const float attack) -> void {
				this->attack_ = attack;
			}

			auto SetMoveSpeed(const float moveSpeed) -> void {
				this->move_speed_ = moveSpeed;
			}

			auto SetMoveAcceleration(const float moveAcceleration) -> void {
				this->move_acceleration_ = moveAcceleration;
			}

			[[nodiscard]] auto GetShape() const -> sf::Shape* {
				return shape_.get();
			}

			[[nodiscard]] auto GetRadius() const -> float {
				return shape_->getRadius();
			}

			[[nodiscard]] auto GetHP() const -> float {
				return this->hp_;
			}

			[[nodiscard]] auto GetAttack() const -> float {
				return this->attack_;
			}

			[[nodiscard]] auto GetPosition() const -> sf::Vector2f {
				return	this->shape_->getPosition();
			}

			virtual auto Draw(sf::RenderWindow& window) const -> void {
				// Obviously, if the NPC or Player has its own rendering logic (e.g., effects rendering),
				// it should be designed as a virtual function for subclasses to override the rendering method.
				if (shape_) {
					window.draw(*shape_);
				}
			}


			// Can not copy a unique ptr
			Entity(const Entity&) = delete;
			Entity& operator=(const Entity&) = delete;
};
}



#endif // ATOM_ENTITY_HPP
