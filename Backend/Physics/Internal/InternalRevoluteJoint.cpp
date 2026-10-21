// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Backend/Physics/Internal/InternalRevoluteJoint.hpp>

#include <Backend/Physics/Internal/InternalPhysicsBody.hpp>
#include <Physics/Dynamics/RigidBody.hpp>

namespace atom::physics {

InternalRevoluteJoint::InternalRevoluteJoint(RevoluteJoint joint, InternalPhysicsBody* a, InternalPhysicsBody* b)
    : joint_(joint), a_(a), b_(b) {
    joint_.a = &a->Body();
    joint_.b = &b->Body();
}

auto InternalRevoluteJoint::GetBodyA() const -> IPhysicsBody* {
    return a_;
}

auto InternalRevoluteJoint::GetBodyB() const -> IPhysicsBody* {
    return b_;
}

auto InternalRevoluteJoint::GetAnchorA() const -> algo::Vec2 {
    return a_->Body().position + joint_.local_anchor_a.Rotated(a_->Body().angle);
}

auto InternalRevoluteJoint::GetAnchorB() const -> algo::Vec2 {
    return b_->Body().position + joint_.local_anchor_b.Rotated(b_->Body().angle);
}

} // namespace atom::physics
