// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file IPhysicsBackend.hpp
 * @brief 物理后端契约。对应 IAudioBackend：一个后端实现一种物理引擎。
 *
 * 游戏代码只依赖 IPhysicsWorld / IPhysicsBody；具体后端（自研 Internal、
 * 将来的 Box2D 适配器）经 Registry 注册、由 BackendRuntime 选中。换库 =
 * 换一个 IPhysicsBackend 实现，契约与游戏代码不变。
 */

#ifndef ATOM_PHYSICS_BACKEND_I_PHYSICS_BACKEND_HPP
#define ATOM_PHYSICS_BACKEND_I_PHYSICS_BACKEND_HPP

#include <memory>
#include <string_view>

namespace atom::physics {

class IPhysicsWorld;

class IPhysicsBackend {
    public:
        virtual ~IPhysicsBackend() = default;

        // 创建独立世界。失败返回 nullptr。
        [[nodiscard]] virtual auto CreateWorld() -> std::unique_ptr<IPhysicsWorld> = 0;
        // 后端注册名，用于日志与设置界面。
        [[nodiscard]] virtual auto Name() const -> std::string_view = 0;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_I_PHYSICS_BACKEND_HPP
