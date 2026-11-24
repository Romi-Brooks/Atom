// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file InternalPhysicsBackend.hpp
 * @brief 自研后端：实现 IPhysicsBackend，创建 InternalPhysicsWorld。
 */

#ifndef ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_BACKEND_HPP
#define ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_BACKEND_HPP

#include <memory>
#include <string_view>

#include <Physics/Contracts/IPhysicsBackend.hpp>

namespace atom::physics {

class InternalPhysicsBackend final : public IPhysicsBackend {
    public:
        [[nodiscard]] auto CreateWorld() -> std::unique_ptr<IPhysicsWorld> override;
        [[nodiscard]] auto Name() const -> std::string_view override;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_BACKEND_HPP
