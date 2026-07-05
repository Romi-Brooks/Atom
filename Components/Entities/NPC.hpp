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

#include <memory>
#include <string>

#include <Config/Components/Entities/NPC.hpp>
#include <Config/Movement/MoveEvent.hpp>
#include <Log/LogSystem.hpp>

#include "Entity.hpp"

enum class NPCName;

class NPC : public atom::Entity {
    protected:
        NPCName name_;
        atom::NPCType npc_type_ {};
        atom::NPCKillable npc_killable_ {};

    public:
        NPC(NPCName name, atom::NPCType type, atom::NPCKillable killable,
            float hp, float attack, float moveSpeed, float moveAcceleration)
        : Entity(hp, attack, moveSpeed, moveAcceleration) {
            name_ = name;
            npc_type_ = type;
            npc_killable_ = killable;
            CreateCircleWithColor(15, atom::Color::Red());
        }

        auto Move(const atom::Movement Signal) const -> void override {
            float x = position_.GetX();
            float y = position_.GetY();

            switch (Signal) {
            case atom::Movement::Entity_MoveLeft:
                position_.SetX(x - (move_speed_ + move_acceleration_));
                break;
            case atom::Movement::Entity_MoveRight:
                position_.SetX(x + (move_speed_ + move_acceleration_));
                break;
            case atom::Movement::Entity_MoveUp:
                position_.SetY(y - (move_speed_ + move_acceleration_));
                break;
            case atom::Movement::Entity_MoveDown:
                position_.SetY(y + (move_speed_ + move_acceleration_));
                break;
            default:
                LOG_ERROR(atom::LogChannel::ATOM_CONFIG_MOVEMENT, "Error Movement sign!");
                break;
            }
        }

        [[nodiscard]] virtual auto GetNPCName() const -> std::string {
            return "";
        }

        [[nodiscard]] auto GetNPCType() const -> atom::NPCType {
            return npc_type_;
        }

        [[nodiscard]] auto GetNPCKillable() const -> atom::NPCKillable {
            return npc_killable_;
        }
};

#endif // ATOM_NPC_HPP
