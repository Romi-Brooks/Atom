// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Backend/Physics/Internal/InternalPhysicsWorld.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <Backend/Physics/Internal/InternalPhysicsBody.hpp>
#include <Backend/Physics/Internal/InternalRevoluteJoint.hpp>
#include <Log/LogSystem.hpp>
#include <Physics/Collision/NarrowPhase.hpp>
#include <Physics/Shapes/Shape.hpp>

namespace atom::physics {

namespace {

// 休眠阈值：连续 0.5s 内线速度 ≤ 0.05 m/s 且角速度 ≤ 0.05 rad/s 即入睡。
// 经验值，后续可暴露为 SleepConfig。
constexpr float kSleepLinearTol = 0.05f;
constexpr float kSleepAngularTol = 0.05f;
constexpr float kSleepTimeThreshold = 0.5f;

[[nodiscard]] inline auto MakePair(InternalPhysicsBody* a, InternalPhysicsBody* b)
    -> std::pair<InternalPhysicsBody*, InternalPhysicsBody*> {
    return a < b ? std::pair<InternalPhysicsBody*, InternalPhysicsBody*>{a, b}
                 : std::pair<InternalPhysicsBody*, InternalPhysicsBody*>{b, a};
}

// 关节约束是否「参与」求解：存在一个醒着的 dynamic 端。
[[nodiscard]] inline auto IsJointActive(const RigidBody& a, const RigidBody& b) -> bool {
    const auto active = [](const RigidBody& rb) { return rb.type == BodyType::Dynamic && !rb.asleep; };
    return active(a) || active(b);
}

// BodyType 的可读名，仅用于低频的日志输出（热路径不调用）。
[[nodiscard]] inline auto BodyTypeName(const BodyType type) -> const char* {
    switch (type) {
    case BodyType::Static:
        return "static";
    case BodyType::Kinematic:
        return "kinematic";
    case BodyType::Dynamic:
        return "dynamic";
    }
    return "unknown";
}

// 刚体的稳定标识（其内部 RigidBody 地址），仅用于低频日志关联同一刚体。
[[nodiscard]] inline auto BodyTag(const RigidBody& body) -> std::string {
    return std::to_string(reinterpret_cast<std::uintptr_t>(&body));
}

} // namespace

InternalPhysicsWorld::InternalPhysicsWorld(const float broad_phase_cell_size) : broad_phase_(broad_phase_cell_size) {
    LOG_INFO(atom::log::physics::World, "Physics world created (broad-phase cell size " +
                                             std::to_string(broad_phase_cell_size) + " m)");
}

InternalPhysicsWorld::~InternalPhysicsWorld() {
    LOG_INFO(atom::log::physics::World, "Physics world destroyed (" + std::to_string(bodies_.size()) + " bodies, " +
                                             std::to_string(joints_.size()) + " joints)");
}

auto InternalPhysicsWorld::CreateBody(const BodyDef& def) -> IPhysicsBody* {
    RigidBody body{};
    body.type = def.type;
    body.shape = def.shape;
    body.material = def.material;
    body.position = def.position;
    body.angle = def.angle;

    const MassProperties mp = ComputeMassProperties(def.shape, def.material.density);
    body.mass = mp.mass;
    body.inverse_mass = mp.inverse_mass;
    body.inertia = mp.inertia;
    body.inverse_inertia = mp.inverse_inertia;
    body.local_center = mp.centroid;

    // 只有 Dynamic 体有有限质量；Static/Kinematic 在求解中视为无限质量，
    // 否则求解器会在单步内"推动"静态体速度（被下步积分清零，能量账目错误）。
    if (body.type != BodyType::Dynamic) {
        body.inverse_mass = 0.0f;
        body.inverse_inertia = 0.0f;
    }

    // 动态体必须有质量可积，否则模拟无定义。
    if (body.type == BodyType::Dynamic && body.inverse_mass == 0.0f) {
        LOG_WARNING(atom::log::physics::Body,
                    "Rejected dynamic body: shape has zero mass (degenerate shape or zero density)");
        return nullptr;
    }

    UpdateWorldAabb(body);

    auto owned = std::make_unique<InternalPhysicsBody>(std::move(body));
    InternalPhysicsBody* raw = owned.get();
    bodies_.push_back(std::move(owned));
    LOG_DEBUG(atom::log::physics::Body, "Created body (" + std::string{BodyTypeName(raw->Body().type)} + ")");
    return raw;
}

auto InternalPhysicsWorld::DestroyBody(IPhysicsBody* body) -> void {
    if (body == nullptr) {
        LOG_WARNING(atom::log::physics::Body, "DestroyBody called with a null body");
        return;
    }
    auto* internal = static_cast<InternalPhysicsBody*>(body);

    // 销毁依附于该刚体的关节（Box2D 同语义：先清关节再清刚体）。
    for (auto it = joints_.begin(); it != joints_.end();) {
        InternalRevoluteJoint* j = it->get();
        if (j->Joint().a == &internal->Body() || j->Joint().b == &internal->Body()) {
            no_collide_pairs_.erase(MakePair(static_cast<InternalPhysicsBody*>(j->GetBodyA()),
                                             static_cast<InternalPhysicsBody*>(j->GetBodyB())));
            it = joints_.erase(it);
        } else {
            ++it;
        }
    }

    // 清掉涉及该刚体的历史接触对，避免悬垂指针 / 误发 EndContact。
    for (auto it = prev_contacts_.begin(); it != prev_contacts_.end();) {
        if (it->first == internal || it->second == internal) {
            it = prev_contacts_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = bodies_.begin(); it != bodies_.end(); ++it) {
        if (it->get() == body) {
            bodies_.erase(it);
            LOG_DEBUG(atom::log::physics::Body, "Destroyed body");
            return;
        }
    }
    LOG_WARNING(atom::log::physics::Body, "DestroyBody: body does not belong to this world");
}

auto InternalPhysicsWorld::FindInternal(IPhysicsBody* body) const -> InternalPhysicsBody* {
    for (const auto& owned : bodies_) {
        if (owned.get() == body) {
            return owned.get();
        }
    }
    return nullptr;
}

auto InternalPhysicsWorld::CreateRevoluteJoint(const RevoluteJointDef& def, IPhysicsBody* a, IPhysicsBody* b)
    -> IPhysicsJoint* {
    InternalPhysicsBody* ia = FindInternal(a);
    InternalPhysicsBody* ib = FindInternal(b);
    if (ia == nullptr || ib == nullptr || ia == ib) {
        LOG_WARNING(atom::log::physics::Body,
                    "Rejected revolute joint: body does not belong to this world, or both anchors are the same body");
        return nullptr;
    }

    RevoluteJoint joint{};
    joint.local_anchor_a = def.local_anchor_a;
    joint.local_anchor_b = def.local_anchor_b;

    auto owned = std::make_unique<InternalRevoluteJoint>(joint, ia, ib);
    InternalRevoluteJoint* raw = owned.get();
    joints_.push_back(std::move(owned));

    if (!def.collide_connected) {
        no_collide_pairs_.insert(MakePair(ia, ib));
    }
    LOG_DEBUG(atom::log::physics::Body, "Created revolute joint");
    return raw;
}

auto InternalPhysicsWorld::DestroyJoint(IPhysicsJoint* joint) -> void {
    if (joint == nullptr) {
        LOG_WARNING(atom::log::physics::Body, "DestroyJoint called with a null joint");
        return;
    }
    for (auto it = joints_.begin(); it != joints_.end(); ++it) {
        if (it->get() == joint) {
            InternalRevoluteJoint* j = it->get();
            no_collide_pairs_.erase(MakePair(static_cast<InternalPhysicsBody*>(j->GetBodyA()),
                                             static_cast<InternalPhysicsBody*>(j->GetBodyB())));
            joints_.erase(it);
            LOG_DEBUG(atom::log::physics::Body, "Destroyed revolute joint");
            return;
        }
    }
    LOG_WARNING(atom::log::physics::Body, "DestroyJoint: joint does not belong to this world");
}

auto InternalPhysicsWorld::Step(const float delta_time, const int velocity_iterations,
                                const int position_iterations) -> void {
    // 输入校验：非法参数是配置错误（正常路径由 FixedStepper 保证合法）。这是
    // 低频边界——只在首次出现时告警一次，避免持续错误配置时每帧刷屏拖慢主线程。
    if (!step_input_warned_ &&
        (delta_time < 0.0f || !std::isfinite(delta_time) || velocity_iterations <= 0 || position_iterations <= 0)) {
        LOG_WARNING(atom::log::physics::World,
                    "Invalid Step parameters (dt=" + std::to_string(delta_time) +
                        ", velocity_iterations=" + std::to_string(velocity_iterations) +
                        ", position_iterations=" + std::to_string(position_iterations) + ")");
        step_input_warned_ = true;
    }

    // 1. 积分速度（休眠体跳过；静态体清零、运动学体只由代码驱动）。
    for (auto& body : bodies_) {
        if (!body->Body().asleep) {
            IntegrateVelocity(body->Body(), delta_time, gravity_);
        }
    }

    // 1.5 关节唤醒（在碰撞/求解前，让被运动关节拖动的休眠体参与本步）。
    WakeOnJoints();

    // 2. 宽相位重建（含休眠体，它们仍是清醒体的障碍物）。
    broad_phase_.Clear();
    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        UpdateWorldAabb(bodies_[i]->Body());
        broad_phase_.Insert(static_cast<int>(i), bodies_[i]->Body().aabb);
    }

    // 3. 窄相位 + 接触收集 + 事件配对 + 接触唤醒。
    std::set<ContactPair> current_contacts{};
    std::vector<ContactSolverInput> contacts{};
    broad_phase_.FindPairs([&](const int i, const int j) {
        RigidBody& body_a = bodies_[i]->Body();
        RigidBody& body_b = bodies_[j]->Body();
        const ContactPair key = MakePair(bodies_[i].get(), bodies_[j].get());

        // 关节明确禁碰的对直接跳过。
        if (no_collide_pairs_.find(key) != no_collide_pairs_.end()) {
            return;
        }

        // 双方都睡着：跳过窄相位（零成本），但保留配对，避免误发 EndContact。
        if (body_a.asleep && body_b.asleep) {
            current_contacts.insert(key);
            return;
        }

        ContactManifold manifold{};
        if (Collide(body_a.shape, body_a.position, body_a.angle, body_b.shape, body_b.position, body_b.angle,
                    manifold)) {
            current_contacts.insert(key);
            contacts.push_back(ContactSolverInput{&body_a, &body_b, manifold});
            WakeOnContact(body_a, body_b, key);
        }
    });

    // 3.5 事件配对：Begin = 当前 − 历史；End = 历史 − 当前。
    if (contact_listener_ != nullptr) {
        for (const ContactPair& key : current_contacts) {
            if (prev_contacts_.find(key) == prev_contacts_.end()) {
                contact_listener_->BeginContact(key.first, key.second);
            }
        }
        for (const ContactPair& key : prev_contacts_) {
            if (current_contacts.find(key) == current_contacts.end()) {
                contact_listener_->EndContact(key.first, key.second);
            }
        }
    }
    prev_contacts_ = std::move(current_contacts);

    // 4. 速度求解（碰撞响应）。
    SolveVelocityContacts(contacts, velocity_iterations, solver_config_);

    // 4.5 关节速度求解（点约束一次 2×2 精确；多次迭代只对多约束耦合有意义）。
    for (int iteration = 0; iteration < velocity_iterations; ++iteration) {
        for (auto& joint : joints_) {
            RevoluteJoint& j = joint->Joint();
            if (!IsJointActive(*j.a, *j.b)) {
                continue;
            }
            if (iteration == 0) {
                j.Prepare();
            }
            j.SolveVelocity();
        }
    }

    // 5. 积分位置（休眠体跳过）。
    for (auto& body : bodies_) {
        if (!body->Body().asleep) {
            IntegratePosition(body->Body(), delta_time);
        }
    }

    // 6. 位置修正（消除残余穿透）。
    SolvePositionContacts(contacts, position_iterations, solver_config_);

    // 6.5 关节位置修正。
    for (int iteration = 0; iteration < position_iterations; ++iteration) {
        for (auto& joint : joints_) {
            RevoluteJoint& j = joint->Joint();
            if (!IsJointActive(*j.a, *j.b)) {
                continue;
            }
            j.Prepare();
            j.SolvePosition();
        }
    }

    // 7. 休眠判定。
    UpdateSleepState(delta_time);
}

auto InternalPhysicsWorld::SetGravity(const algo::Vec2 gravity) -> void {
    if (!std::isfinite(gravity.GetX()) || !std::isfinite(gravity.GetY())) {
        LOG_WARNING(atom::log::physics::World, "Ignored non-finite gravity (NaN/Inf)");
        return;
    }
    gravity_ = gravity;
    LOG_INFO(atom::log::physics::World, "Gravity set to (" + std::to_string(gravity.GetX()) + ", " +
                                             std::to_string(gravity.GetY()) + ")");
}

auto InternalPhysicsWorld::GetGravity() const -> algo::Vec2 {
    return gravity_;
}

auto InternalPhysicsWorld::SetContactListener(IContactListener* listener) -> void {
    contact_listener_ = listener;
}

auto InternalPhysicsWorld::SetSleepingEnabled(const bool enabled) -> void {
    sleeping_enabled_ = enabled;
    LOG_INFO(atom::log::physics::World, enabled ? "Sleeping enabled" : "Sleeping disabled");
    if (!enabled) {
        // 关闭休眠：唤醒全部刚体，之后不再入睡。
        for (auto& body : bodies_) {
            body->Body().asleep = false;
            body->Body().sleep_timer = 0.0f;
        }
    }
}

auto InternalPhysicsWorld::WakeOnJoints() -> void {
    for (auto& joint : joints_) {
        RevoluteJoint& j = joint->Joint();
        const auto pulls = [](const RigidBody& rb) {
            return rb.type == BodyType::Kinematic || (rb.type == BodyType::Dynamic && !rb.asleep);
        };
        const auto wake = [](RigidBody& rb) {
            if (rb.type == BodyType::Dynamic && rb.asleep) {
                rb.asleep = false;
                rb.sleep_timer = 0.0f;
                LOG_DEBUG(atom::log::physics::Body, "Body woken by joint #" + BodyTag(rb));
            }
        };
        if (pulls(*j.a)) {
            wake(*j.b);
        }
        if (pulls(*j.b)) {
            wake(*j.a);
        }
    }
}

auto InternalPhysicsWorld::WakeOnContact(RigidBody& a, RigidBody& b, const ContactPair& key) -> void {
    const bool is_new = prev_contacts_.find(key) == prev_contacts_.end();
    const float lin2 = kSleepLinearTol * kSleepLinearTol;
    const float ang2 = kSleepAngularTol * kSleepAngularTol;

    const auto wake_if_needed = [&](RigidBody& sleeping, const RigidBody& other) {
        if (!sleeping.asleep) {
            return;
        }
        // 静态障碍物永不唤醒（否则静置堆叠永远睡不着）。
        if (other.type == BodyType::Static) {
            return;
        }
        const bool moving = other.linear_velocity.LengthSquared() > lin2 || other.angular_velocity * other.angular_velocity > ang2;
        if (is_new || moving) {
            sleeping.asleep = false;
            sleeping.sleep_timer = 0.0f;
            LOG_DEBUG(atom::log::physics::Body, "Body woken by contact #" + BodyTag(sleeping));
        }
    };

    wake_if_needed(a, b);
    wake_if_needed(b, a);
}

auto InternalPhysicsWorld::UpdateSleepState(const float delta_time) -> void {
    if (!sleeping_enabled_) {
        return;
    }
    const float lin2 = kSleepLinearTol * kSleepLinearTol;
    const float ang2 = kSleepAngularTol * kSleepAngularTol;

    for (auto& body : bodies_) {
        RigidBody& rb = body->Body();
        if (rb.type != BodyType::Dynamic || rb.asleep) {
            continue;
        }
        const bool low_energy = rb.linear_velocity.LengthSquared() <= lin2 &&
                                rb.angular_velocity * rb.angular_velocity <= ang2;
        if (low_energy) {
            rb.sleep_timer += delta_time;
            if (rb.sleep_timer >= kSleepTimeThreshold) {
                rb.asleep = true;
                rb.linear_velocity = algo::Vec2::Zero();
                rb.angular_velocity = 0.0f;
                LOG_DEBUG(atom::log::physics::Body, "Body fell asleep #" + BodyTag(rb));
            }
        } else {
            rb.sleep_timer = 0.0f;
        }
    }
}

} // namespace atom::physics
