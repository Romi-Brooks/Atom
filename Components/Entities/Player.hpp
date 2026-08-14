/**
  * @file           : Player.hpp
  * @author         : Romi Brooks
  * @brief          :
  * @attention      :
  * @date           : 2025/9/20
  Copyright (c) 2025 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_PLAYER_HPP
#define ATOM_PLAYER_HPP

#include <string>

#include <Config/Movement/MoveEvent.hpp>

#include "Entity.hpp"

class Player : public atom::Entity {
private:
    std::string name_;
    unsigned int money_;
    unsigned short level_ = 1;
    unsigned short exp_ = 0;

    auto UpdateProperty() -> bool {
        attack_ = static_cast<float>(level_) * 7;
        return true;
    }

public:
    Player(std::string name, float hp, float attack, float moveSpeed, float moveAcceleration)
        : Entity(hp, attack, moveSpeed, moveAcceleration), name_(std::move(name)), money_(50) {
        CreateCircleWithColor(20, atom::Color::White());
    }

    auto Draw(atom::IRenderTarget& target) const -> void override {
        Entity::Draw(target);
    }

    auto LevelUp() -> void {
        ++level_;
    }

    auto AddMoney(unsigned int value) -> void {
        money_ += value;
    }

    auto AddExp(unsigned int exp) -> void {
        exp_ += exp;
    }

    auto Move(const atom::Movement Signal) const -> void override {
        float x = position_.GetX();
        float y = position_.GetY();

        switch (Signal) {
        case atom::Movement::Entity_MoveLeft:
            position_.SetX(x - 3);
            break;
        case atom::Movement::Entity_MoveRight:
            position_.SetX(x + 3);
            break;
        case atom::Movement::Entity_MoveUp:
            position_.SetY(y - 3);
            break;
        case atom::Movement::Entity_MoveDown:
            position_.SetY(y + 3);
            break;
        default:
            break;
        }
    }

    [[nodiscard]] auto GetPlayerName() const -> const std::string& {
        return name_;
    }

    [[nodiscard]] auto GetPlayerMoney() const -> unsigned int {
        return money_;
    }

    [[nodiscard]] auto GetPlayerLevel() const -> unsigned short {
        return level_;
    }

    [[nodiscard]] auto GetPlayerExp() const -> unsigned short {
        return exp_;
    }
};

#endif // ATOM_PLAYER_HPP
