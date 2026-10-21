// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file InternalRevoluteJoint.hpp
 * @brief Internal 后端的 revolute 关节：包装 RevoluteJoint，实现 IPhysicsJoint。
 */

#ifndef ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_REVOLUTE_JOINT_HPP
#define ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_REVOLUTE_JOINT_HPP

#include <Backend/Contracts/Physics/IPhysicsJoint.hpp>
#include <Physics/Solver/RevoluteJoint.hpp>

namespace atom::physics {

class InternalPhysicsBody;

class InternalRevoluteJoint final : public IPhysicsJoint {
    public:
        InternalRevoluteJoint(RevoluteJoint joint, InternalPhysicsBody* a, InternalPhysicsBody* b);

        [[nodiscard]] auto GetBodyA() const -> IPhysicsBody* override;
        [[nodiscard]] auto GetBodyB() const -> IPhysicsBody* override;
        [[nodiscard]] auto GetAnchorA() const -> algo::Vec2 override;
        [[nodiscard]] auto GetAnchorB() const -> algo::Vec2 override;

        [[nodiscard]] auto Joint() -> RevoluteJoint& {
            return joint_;
        }
        [[nodiscard]] auto Joint() const -> const RevoluteJoint& {
            return joint_;
        }

    private:
        RevoluteJoint joint_{};
        InternalPhysicsBody* a_ = nullptr;
        InternalPhysicsBody* b_ = nullptr;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_REVOLUTE_JOINT_HPP
