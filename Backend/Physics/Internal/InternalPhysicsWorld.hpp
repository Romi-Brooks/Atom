// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file InternalPhysicsWorld.hpp
 * @brief Internal 后端的物理世界：刚体生命周期 + 积分 + 宽相位 + 接触求解 + 休眠。
 *
 * Step 顺序：积分速度 → 宽/窄相位（收集接触 + Begin/End 事件配对 + 接触唤醒）
 * → 速度求解 → 积分位置 → 位置修正 → 休眠判定。休眠体跳过积分与窄相位，
 * 全休眠堆叠的 Step 开销趋近于零。
 */

#ifndef ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_WORLD_HPP
#define ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_WORLD_HPP

#include <memory>
#include <set>
#include <vector>

#include <Backend/Contracts/Physics/IPhysicsWorld.hpp>
#include <Physics/Collision/BroadPhase.hpp>
#include <Physics/Solver/ContactSolver.hpp>

namespace atom::physics {

class InternalPhysicsBody;
class InternalRevoluteJoint;

class InternalPhysicsWorld final : public IPhysicsWorld {
    public:
        // broad_phase_cell_size 接近物体平均尺寸（米）。
        explicit InternalPhysicsWorld(float broad_phase_cell_size = 1.0f);
        // 显式声明并在 .cpp 定义：成员 vector<unique_ptr<InternalPhysicsBody>>
        // 的析构需要 InternalPhysicsBody 完整类型，头里只有前向声明。
        ~InternalPhysicsWorld() override;

        [[nodiscard]] auto CreateBody(const BodyDef& def) -> IPhysicsBody* override;
        auto DestroyBody(IPhysicsBody* body) -> void override;
        [[nodiscard]] auto CreateRevoluteJoint(const RevoluteJointDef& def, IPhysicsBody* a,
                                               IPhysicsBody* b) -> IPhysicsJoint* override;
        auto DestroyJoint(IPhysicsJoint* joint) -> void override;
        auto Step(float delta_time, int velocity_iterations, int position_iterations) -> void override;
        auto SetGravity(algo::Vec2 gravity) -> void override;
        [[nodiscard]] auto GetGravity() const -> algo::Vec2 override;
        auto SetContactListener(IContactListener* listener) -> void override;
        auto SetSleepingEnabled(bool enabled) -> void override;

    private:
        using ContactPair = std::pair<InternalPhysicsBody*, InternalPhysicsBody*>;

        // 接触唤醒：接触双方若一方休眠，且另一方是「新接触」或「醒着且有实质速度」的非静态体，则唤醒。
        auto WakeOnContact(RigidBody& a, RigidBody& b, const ContactPair& key) -> void;
        // 关节唤醒：关节一端是 kinematic / 醒着 dynamic 时，唤醒另一端。
        auto WakeOnJoints() -> void;
        // 休眠判定：低速度累积到阈值则入睡并清零速度。
        auto UpdateSleepState(float delta_time) -> void;
        // 在 bodies_ 中查找 InternalPhysicsBody*，未找到返回 nullptr。
        [[nodiscard]] auto FindInternal(IPhysicsBody* body) const -> InternalPhysicsBody*;
        // Step 内的接触回调可以请求销毁刚体。真正释放必须延后到本步完成，
        // 否则求解器持有的 ContactSolverInput 会悬垂。
        auto DestroyBodyImmediate(InternalPhysicsBody* body) -> void;
        auto FlushDeferredBodyDestroys() -> void;

        std::vector<std::unique_ptr<InternalPhysicsBody>> bodies_{};
        std::vector<std::unique_ptr<InternalRevoluteJoint>> joints_{};
        UniformGrid broad_phase_;
        ContactSolverConfig solver_config_{};
        algo::Vec2 gravity_{0.0f, -9.8f};
        IContactListener* contact_listener_ = nullptr;
        bool sleeping_enabled_ = true;
        // 无效 Step 参数只告警一次（避免持续错误配置时每帧刷屏）。
        bool step_input_warned_ = false;
        bool stepping_ = false;
        std::set<ContactPair> prev_contacts_{};
        std::set<ContactPair> no_collide_pairs_{}; // collide_connected=false 的关节对
        std::vector<InternalPhysicsBody*> deferred_body_destroys_{};
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_BACKEND_INTERNAL_INTERNAL_PHYSICS_WORLD_HPP
