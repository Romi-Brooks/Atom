// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Backend/Internal/InternalPhysicsWorld.hpp>

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

#include <Physics/Backend/Internal/InternalPhysicsBody.hpp>
#include <Physics/Backend/Internal/InternalRevoluteJoint.hpp>
#include <Concurrency/JobSystem.hpp>
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
// SAT work for a single candidate is deliberately small. Avoid scheduling
// overhead on ordinary scenes; large candidate batches are range-partitioned
// rather than creating one job per pair.
constexpr std::size_t kNarrowPhaseParallelThreshold = 128;
constexpr std::size_t kNarrowPhaseMinimumGrain = 32;

struct NarrowPhaseResult final {
        bool preserve_sleeping_contact = false;
        bool hit = false;
        ContactManifold manifold{};
};

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
    const auto finite_vec = [](const algo::Vec2 value) {
        return std::isfinite(value.GetX()) && std::isfinite(value.GetY());
    };
    if (!finite_vec(def.position) || !std::isfinite(def.angle) || !std::isfinite(def.material.density) ||
        !std::isfinite(def.material.friction) || !std::isfinite(def.material.restitution) ||
        def.material.density < 0.0f || def.material.friction < 0.0f || def.material.restitution < 0.0f ||
        def.material.restitution > 1.0f) {
        LOG_WARNING(atom::log::physics::Body, "Rejected body with non-finite or invalid material/transform");
        return nullptr;
    }

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

    if (!std::isfinite(body.mass) || !std::isfinite(body.inertia) || !std::isfinite(body.inverse_mass) ||
        !std::isfinite(body.inverse_inertia) || !finite_vec(body.local_center)) {
        LOG_WARNING(atom::log::physics::Body, "Rejected body with non-finite mass properties");
        return nullptr;
    }

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
    InternalPhysicsBody* internal = FindInternal(body);
    if (internal == nullptr) {
        LOG_WARNING(atom::log::physics::Body, "DestroyBody: body does not belong to this world");
        return;
    }
    if (stepping_) {
        if (std::ranges::find(deferred_body_destroys_, internal) == deferred_body_destroys_.end()) {
            deferred_body_destroys_.push_back(internal);
        }
        return;
    }
    DestroyBodyImmediate(internal);
}

auto InternalPhysicsWorld::DestroyBodyImmediate(InternalPhysicsBody* internal) -> void {

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
        if (it->get() == internal) {
            bodies_.erase(it);
            LOG_DEBUG(atom::log::physics::Body, "Destroyed body");
            return;
        }
    }
}

auto InternalPhysicsWorld::FlushDeferredBodyDestroys() -> void {
    for (InternalPhysicsBody* body : deferred_body_destroys_) {
        if (FindInternal(body) != nullptr) {
            DestroyBodyImmediate(body);
        }
    }
    deferred_body_destroys_.clear();
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
    if (stepping_) {
        LOG_WARNING(atom::log::physics::World, "Ignored recursive PhysicsWorld::Step call");
        return;
    }
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
    if (delta_time < 0.0f || !std::isfinite(delta_time) || velocity_iterations <= 0 || position_iterations <= 0) {
        return;
    }

    stepping_ = true;

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

    // 3. 先稳定收集候选对。UniformGrid guarantees the list is sorted by
    // body index, so the later merge (and therefore solver order) stays
    // deterministic even when Collide itself runs on workers.
    std::vector<std::pair<int, int>> candidates{};
    broad_phase_.FindPairs([&](const int i, const int j) { candidates.emplace_back(i, j); });

    // 3.5 窄相位：每个 range 只写自己的 result 槽，既没有共享可变状态，
    // 也不允许 worker 触发唤醒/事件。WaitAll 后按候选索引串行归并。
    std::vector<NarrowPhaseResult> narrow_results(candidates.size());
    const auto narrow_range = [&](const std::size_t begin, const std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            const auto [i, j] = candidates[index];
            RigidBody& body_a = bodies_[i]->Body();
            RigidBody& body_b = bodies_[j]->Body();
            const ContactPair key = MakePair(bodies_[i].get(), bodies_[j].get());

            if (no_collide_pairs_.contains(key)) {
                continue;
            }

            NarrowPhaseResult& result = narrow_results[index];
            if (body_a.asleep && body_b.asleep) {
                result.preserve_sleeping_contact = prev_contacts_.contains(key);
                continue;
            }
            result.hit = Collide(body_a.shape, body_a.position, body_a.angle, body_b.shape, body_b.position,
                                 body_b.angle, result.manifold);
        }
    };

    auto& jobs = atom::job::JobSystem::GetInstance();
    const bool use_parallel_narrow = jobs.IsRunning() && jobs.WorkerCount() > 1 &&
                                     candidates.size() >= kNarrowPhaseParallelThreshold;
    if (use_parallel_narrow) {
        const std::size_t target_jobs = jobs.WorkerCount() * 4;
        const std::size_t grain =
            std::max(kNarrowPhaseMinimumGrain, (candidates.size() + target_jobs - 1) / target_jobs);
        std::vector<atom::job::JobHandle> handles{};
        handles.reserve((candidates.size() + grain - 1) / grain);
        for (std::size_t begin = 0; begin < candidates.size(); begin += grain) {
            const std::size_t end = std::min(begin + grain, candidates.size());
            handles.push_back(jobs.Schedule("physics:narrow", [&, begin, end] { narrow_range(begin, end); }));
        }
        jobs.WaitAll(handles);
    } else {
        narrow_range(0, candidates.size());
    }

    // 3.6 确定性归并：接触收集、事件和唤醒仍只在编排线程发生。
    std::set<ContactPair> current_contacts{};
    std::vector<ContactSolverInput> contacts{};
    contacts.reserve(candidates.size());
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto [i, j] = candidates[index];
        RigidBody& body_a = bodies_[i]->Body();
        RigidBody& body_b = bodies_[j]->Body();
        const ContactPair key = MakePair(bodies_[i].get(), bodies_[j].get());
        const NarrowPhaseResult& result = narrow_results[index];
        if (result.preserve_sleeping_contact) {
            current_contacts.insert(key);
        } else if (result.hit) {
            current_contacts.insert(key);
            contacts.push_back(ContactSolverInput{&body_a, &body_b, result.manifold});
            WakeOnContact(body_a, body_b, key);
        }
    }

    // 3.7 事件配对：Begin = 当前 − 历史；End = 历史 − 当前。
    // Snapshot the listener for this dispatch. A callback may replace or clear
    // the listener, which must not invalidate the remainder of this event pass.
    IContactListener* const listener = contact_listener_;
    if (listener != nullptr) {
        for (const ContactPair& key : current_contacts) {
            if (prev_contacts_.find(key) == prev_contacts_.end()) {
                listener->BeginContact(key.first, key.second);
            }
        }
        for (const ContactPair& key : prev_contacts_) {
            if (current_contacts.find(key) == current_contacts.end()) {
                listener->EndContact(key.first, key.second);
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
    stepping_ = false;
    FlushDeferredBodyDestroys();
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
            }
        } else {
            rb.sleep_timer = 0.0f;
        }
    }
}

} // namespace atom::physics
