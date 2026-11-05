// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file IPhysicsWorld.hpp
 * @brief 物理世界契约 + 碰撞事件监听器。
 *
 * 世界拥有刚体的生命周期；固定步 Step 驱动整个模拟。碰撞 Begin/End 事件
 * 与休眠控制在 M5 落地；查询（射线/形状/区域）属于后续里程碑，追加到本接口即可。
 */

#ifndef ATOM_PHYSICS_BACKEND_I_PHYSICS_WORLD_HPP
#define ATOM_PHYSICS_BACKEND_I_PHYSICS_WORLD_HPP

#include <Algorithm/Vector/Vec2.hpp>
#include <Backend/Contracts/Physics/PhysicsTypes.hpp>

namespace atom::physics {

class IPhysicsBody;
class IPhysicsJoint;

// 碰撞生命周期回调。默认空实现，游戏按需覆写。回调发生在 Step 期间，
// 世界处于一致的模拟状态，但不保证刚体顺序。回调内请求 DestroyBody
// 会在当前 Step 完成后实际释放，保证本步求解器引用仍有效。
class IContactListener {
    public:
        virtual ~IContactListener() = default;
        virtual auto BeginContact(IPhysicsBody* a, IPhysicsBody* b) -> void {}
        virtual auto EndContact(IPhysicsBody* a, IPhysicsBody* b) -> void {}
};

class IPhysicsWorld {
    public:
        virtual ~IPhysicsWorld() = default;

        // 返回 nullptr 表示创建失败（非法 BodyDef、后端限制）。
        [[nodiscard]] virtual auto CreateBody(const BodyDef& def) -> IPhysicsBody* = 0;
        virtual auto DestroyBody(IPhysicsBody* body) -> void = 0;

        // 关节：创建失败返回 nullptr（非法 def / 后端不支持）。
        [[nodiscard]] virtual auto CreateRevoluteJoint(const RevoluteJointDef& def, IPhysicsBody* a,
                                                       IPhysicsBody* b) -> IPhysicsJoint* = 0;
        virtual auto DestroyJoint(IPhysicsJoint* joint) -> void = 0;

        // 推进模拟。velocity_iterations / position_iterations 分离，是调稳定性的两个旋钮。
        virtual auto Step(float delta_time, int velocity_iterations, int position_iterations) -> void = 0;

        virtual auto SetGravity(algo::Vec2 gravity) -> void = 0;
        [[nodiscard]] virtual auto GetGravity() const -> algo::Vec2 = 0;

        virtual auto SetContactListener(IContactListener* listener) -> void = 0;

        // 休眠开关（默认开）。关闭时唤醒全部刚体且不再入睡。
        virtual auto SetSleepingEnabled(bool enabled) -> void = 0;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_I_PHYSICS_WORLD_HPP
