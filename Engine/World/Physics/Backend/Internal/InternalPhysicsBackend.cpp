// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Backend/Internal/InternalPhysicsBackend.hpp>

#include <Physics/Backend/Internal/InternalPhysicsWorld.hpp>
#include <Log/LogSystem.hpp>

namespace atom::physics {

auto InternalPhysicsBackend::CreateWorld() -> std::unique_ptr<IPhysicsWorld> {
    LOG_DEBUG(atom::log::physics::Backend, "Internal backend created a new physics world");
    return std::make_unique<InternalPhysicsWorld>();
}

auto InternalPhysicsBackend::Name() const -> std::string_view {
    return "internal";
}

} // namespace atom::physics
