// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Integration/Physics2DWorld.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Log/LogSystem.hpp>
#include <Physics/Contracts/IPhysicsBody.hpp>
#include <Physics/Shapes/Shape.hpp>

namespace atom::physics::integration {

namespace {

using ecs::Entity;
using ecs::EntityWorld;
using ecs::PhysicsBody2D;
using ecs::PhysicsBodyKind;
using ecs::Transform2D;

constexpr std::uint32_t kInvalidHandle = 0;

// (generation << 32) | index. Deliberately hand-rolled rather than reusing the backend's
// integral encoding: the physics contract must not depend on which ECS backend is in use.
auto EncodeEntity(const Entity entity) -> std::uint64_t {
    return (static_cast<std::uint64_t>(entity.Generation()) << 32U) | static_cast<std::uint64_t>(entity.Index());
}

auto DecodeEntity(const std::uint64_t encoded) -> Entity {
    return Entity::FromParts(static_cast<std::uint32_t>(encoded & 0xFFFFFFFFULL),
                             static_cast<std::uint32_t>(encoded >> 32U));
}

auto ToBodyType(const PhysicsBodyKind kind) -> BodyType {
    switch (kind) {
        case PhysicsBodyKind::Static:
            return BodyType::Static;
        case PhysicsBodyKind::Kinematic:
            return BodyType::Kinematic;
        case PhysicsBodyKind::Dynamic:
            return BodyType::Dynamic;
    }
    return BodyType::Static;
}

// One shape per body: a circle when radius > 0, otherwise an axis-aligned box built from
// half_extent. The polygon must be counter-clockwise (the shape factory rejects the
// alternatives), which is why the vertex order looks like this.
auto MakeShape(const PhysicsBody2D& body) -> Result<Shape> {
    if (body.radius > 0.0F)
        return Shape::MakeCircle(body.radius);

    const float hx = std::max(body.half_extent.GetX(), 1.0e-4F);
    const float hy = std::max(body.half_extent.GetY(), 1.0e-4F);
    const std::array<algo::Vec2, 4> vertices{algo::Vec2{-hx, -hy}, algo::Vec2{hx, -hy}, algo::Vec2{hx, hy},
                                             algo::Vec2{-hx, hy}};
    return Shape::MakePolygon(vertices);
}

// Bridges the physics callback to a per-step list. It only records; deciding what an event
// *means* is the caller's job.
class StepContactRecorder final : public IContactListener {
    public:
        explicit StepContactRecorder(std::vector<ContactEvent2D>& out) : out_(&out) {}

        auto BeginContact(IPhysicsBody* a, IPhysicsBody* b) -> void override {
            Record(ContactPhase::Begin, a, b);
        }
        auto PersistContact(IPhysicsBody* a, IPhysicsBody* b) -> void override {
            Record(ContactPhase::Persist, a, b);
        }
        auto EndContact(IPhysicsBody* a, IPhysicsBody* b) -> void override {
            Record(ContactPhase::End, a, b);
        }

    private:
        auto Record(const ContactPhase phase, const IPhysicsBody* a, const IPhysicsBody* b) -> void {
            if (a == nullptr || b == nullptr)
                return;
            ContactEvent2D event{};
            event.phase = phase;
            event.a = DecodeEntity(a->GetUserData());
            event.b = DecodeEntity(b->GetUserData());
            event.sensor = a->IsSensor() || b->IsSensor();
            out_->push_back(event);
        }

        std::vector<ContactEvent2D>* out_ = nullptr;
};

} // namespace

struct Physics2DWorld::Impl final {
        std::unique_ptr<IPhysicsWorld> world{};
        // entity -> body, so the per-frame write-back does not scan every entity that has a
        // shell just to find the tracked ones.
        std::unordered_map<std::uint64_t, IPhysicsBody*> body_by_entity{};
        std::vector<ContactEvent2D> contacts{};
        StepContactRecorder recorder{contacts};
};

auto Physics2DWorld::Create(IPhysicsBackend& backend, const float pixels_per_meter)
    -> std::unique_ptr<Physics2DWorld> {
    auto world = backend.CreateWorld();
    if (world == nullptr)
        return nullptr;

    auto impl = std::make_unique<Impl>();
    impl->world = std::move(world);
    // Physics speaks metres and y-up; the world is pixels and y-down. -9.8 on the physics
    // side is therefore "down" on screen once the conversion in ToPixels/ToMeters runs.
    impl->world->SetGravity({0.0F, -9.81F});
    impl->world->SetContactListener(&impl->recorder);

    auto result = std::unique_ptr<Physics2DWorld>{new Physics2DWorld(std::move(impl))};
    result->pixels_per_meter_ = pixels_per_meter > 0.0F ? pixels_per_meter : 100.0F;
    return result;
}

Physics2DWorld::Physics2DWorld(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Physics2DWorld::~Physics2DWorld() = default;

auto Physics2DWorld::Gravity() const -> algo::Vec2 {
    return impl_->world->GetGravity();
}

auto Physics2DWorld::SetGravity(const algo::Vec2 meters_per_second_squared) -> void {
    impl_->world->SetGravity(meters_per_second_squared);
}

auto Physics2DWorld::ToMeters(const algo::Vec2 pixels) const -> algo::Vec2 {
    return {pixels.GetX() / pixels_per_meter_, -pixels.GetY() / pixels_per_meter_};
}

auto Physics2DWorld::ToPixels(const algo::Vec2 meters) const -> algo::Vec2 {
    return {meters.GetX() * pixels_per_meter_, -meters.GetY() * pixels_per_meter_};
}

auto Physics2DWorld::SetLinearVelocity(const ecs::Entity entity, const algo::Vec2 pixels_per_second) -> bool {
    const auto found = impl_->body_by_entity.find(EncodeEntity(entity));
    if (found == impl_->body_by_entity.end())
        return false;
    found->second->SetLinearVelocity(ToMeters(pixels_per_second));
    return true;
}

auto Physics2DWorld::GetLinearVelocity(const ecs::Entity entity) const -> algo::Vec2 {
    const auto found = impl_->body_by_entity.find(EncodeEntity(entity));
    if (found == impl_->body_by_entity.end())
        return algo::Vec2::Zero();
    return ToPixels(found->second->GetLinearVelocity());
}

auto Physics2DWorld::Step(ecs::EntityWorld& entities, const float fixed_delta_seconds)
    -> Result<std::vector<ContactEvent2D>> {
    if (!(fixed_delta_seconds > 0.0F)) {
        return std::unexpected<Error>{MakeError(ErrorCode::InvalidArgument, Module::Physics, "Physics2DWorld::Step")
                                          .WithDetail("the fixed delta must be positive")};
    }


    // --- 1. create bodies for new shells ---------------------------------
    for (const auto entity : entities.With<Transform2D, PhysicsBody2D>()) {
        auto* shell = entities.TryGet<PhysicsBody2D>(entity);
        const auto* transform = entities.TryGet<Transform2D>(entity);
        if (shell == nullptr || transform == nullptr || shell->handle != kInvalidHandle)
            continue;
        if (impl_->body_by_entity.contains(EncodeEntity(entity)))
            continue;

        auto shape = MakeShape(*shell);
        if (!shape.has_value()) {
            return std::unexpected<Error>{MakeError(ErrorCode::PhysicsInvalidShape, Module::Physics,
                                                   "Physics2DWorld::Step")
                                              .WithDetail("entity body shape rejected: " +
                                                          std::string{shape.error().Message()})};
        }

        BodyDef def{};
        def.shape = *shape;
        def.type = ToBodyType(shell->kind);
        def.position = ToMeters(transform->position);
        def.angle = -transform->rotation; // y-down on screen flips the sense of "CCW"
        def.material.density = shell->density;
        def.material.friction = shell->friction;
        def.material.restitution = shell->restitution;
        def.is_sensor = shell->is_sensor;
        def.user_data = EncodeEntity(entity);

        auto* body = impl_->world->CreateBody(def);
        if (body == nullptr) {
            return std::unexpected<Error>{MakeError(ErrorCode::PhysicsBodyCreationFailed, Module::Physics,
                                                   "Physics2DWorld::Step")
                                              .WithDetail("the physics backend rejected the body definition")};
        }
        shell->handle = 1;
        impl_->body_by_entity.emplace(EncodeEntity(entity), body);
    }

    // --- 2. drop bodies whose entity died --------------------------------
    //
    // ECS has no destruction callback, so this is a scan of the tracked set. It is small
    // (one entry per body) and it is the only way to guarantee that a destroyed entity does
    // not leave an invisible collider behind.
    for (auto it = impl_->body_by_entity.begin(); it != impl_->body_by_entity.end();) {
        if (entities.IsAlive(DecodeEntity(it->first))) {
            ++it;
            continue;
        }
        impl_->world->DestroyBody(it->second);
        it = impl_->body_by_entity.erase(it);
    }
    bodies_.clear();
    bodies_.reserve(impl_->body_by_entity.size());

    // --- 3. step ---------------------------------------------------------
    impl_->contacts.clear();
    impl_->world->Step(fixed_delta_seconds, 8, 3);

    // --- 4. write simulation results back onto the entity transforms -----
    //
    // The physics world is authoritative for a dynamic body: if the ECS transform were also
    // written by gameplay, the two would fight and the body would jitter. Kinematic bodies
    // are the reverse (code moves them, physics reports where they ended up).
    for (auto& [encoded, body] : impl_->body_by_entity) {
        const auto entity = DecodeEntity(encoded);
        auto* transform = entities.TryGet<Transform2D>(entity);
        auto* shell = entities.TryGet<PhysicsBody2D>(entity);
        if (transform == nullptr || shell == nullptr)
            continue;

        if (shell->kind == PhysicsBodyKind::Dynamic)
            transform->position = ToPixels(body->GetPosition());
        transform->rotation = -body->GetAngle();
        // A kinematic body is driven by gameplay: push its transform into physics so the
        // body pushes others correctly.
        if (shell->kind == PhysicsBodyKind::Kinematic) {
            body->SetTransform(ToMeters(transform->position), -transform->rotation);
            body->SetLinearVelocity({0.0F, 0.0F});
        }
        bodies_.push_back(entity);
    }

    // --- 5. hand the contacts over ---------------------------------------
    std::vector<ContactEvent2D> events = std::move(impl_->contacts);
    impl_->contacts.clear();
    return events;
}

} // namespace atom::physics::integration
