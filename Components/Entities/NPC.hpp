/**
  * @file           : NPC.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_NPC_HPP
#define ATOM_NPC_HPP

// Standard Library
#include <memory>
#include <string>

// Third Party Library
#include <SFML/Graphics.hpp>

// Project Headers
#include <Config/Components/Entities/NPC.hpp>
#include <Config/Movement/MoveEvent.hpp>
#include <Log/LogSystem.hpp>

// Self Dependency
#include "Entity.hpp"

enum class NPCName;

class NPC : public atom::Entity{
	private:

	protected:
		// NPC Properties
		NPCName name_;

		// NPC Types
		atom::NPCType npc_type_ {};
		atom::NPCKillable npc_killable_ {};

	public:
		NPC(const NPCName name, const atom::NPCType type, const atom::NPCKillable killable, const float hp, const float attack, const float moveSpeed, const float moveAcceleration)
		: Entity(hp, attack, moveSpeed, moveAcceleration) {
			name_ = name;
			npc_type_ = type;
			npc_killable_ = killable;
			// Give a test Shape
			// For Test Only
			this->CreateCircleWithColor(15, sf::Color::Red);
		};

		auto Move(const atom::Movement Signal) const -> void override {
			auto x = this->shape_->getPosition().x;
			auto y = this->shape_->getPosition().y;

			switch (Signal) {
				case atom::Movement::Entity_MoveLeft: {
					this->shape_->setPosition({x - (this->move_speed_ + this->move_acceleration_), y});
					break;
				}
				case atom::Movement::Entity_MoveRight: {
					this->shape_->setPosition({x + (this->move_speed_ + this->move_acceleration_), y});
					break;
				}
				case atom::Movement::Entity_MoveUp: {
					this->shape_->setPosition({x, y - (this->move_speed_ + this->move_acceleration_)});
					break;
				}
				case atom::Movement::Entity_MoveDown: {
					this->shape_->setPosition({x, y + (this->move_speed_ + this->move_acceleration_)});
					break;
				}
				default: {
					LOG_ERROR(atom::LogChannel::ATOM_CONFIG_MOVEMENT,"Error Movement sign!");
					break;
				}
			}
		}

		[[nodiscard]] virtual auto GetNPCName() const -> std::string {
			// for subclass
			return "";
		}
		[[nodiscard]] auto GetNPCType() const -> atom::NPCType {
				return this->npc_type_;
		}

		[[nodiscard]] auto GetNPCKillable() const -> atom::NPCKillable {
				return this->npc_killable_;
		}
};


#endif // ATOM_NPC_HPP
