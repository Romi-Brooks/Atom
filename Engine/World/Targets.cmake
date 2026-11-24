# ECS — engine-owned entity/component storage.
#     Atom_3rd_EnTT is PRIVATE on purpose: the backend must not reach any public
#     header, and a PUBLIC link would put its include directory on the path of
#     every Atom_ECS consumer, turning the boundary into a comment.
add_library(Atom_ECS STATIC
        Engine/World/ECS/Entity.hpp
        Engine/World/ECS/TypeName.hpp
        Engine/World/ECS/Components.hpp
        Engine/World/ECS/EntityWorld.hpp
        Engine/World/ECS/EntityWorld.cpp
)
target_include_directories(Atom_ECS PUBLIC "${PROJECT_SOURCE_DIR}/Engine/World")
# A0.1: Components.hpp is a public header that includes Engine/Core/Algorithm/Vector/Vec2.hpp
# and Engine/Core/Algorithm/Color/Color.hpp, so both value targets must be PUBLIC -
# previously only Atom_Algorithm was linked PRIVATE, which meant the dependency
# was real but undeclared. Atom_Math is the leaf that now owns Vec2, so ECS no
# longer reaches the transform/camera module.
# Admission rules for ATOM_ECS_COMPONENTS: Docs/Plans/Specs/ECS-Component-Vocabulary-CN.md
target_link_libraries(Atom_ECS PUBLIC Atom_Error Atom_Log Atom_Math Atom_Color)
target_link_libraries(Atom_ECS PRIVATE Atom_3rd_EnTT)

# Physics — collision shapes, mass properties, and the replaceable-backend
#     contract. M1 ships pure shape/mass math + the IPhysics* contract; the
#     Internal solver and world land in later phases (see Docs/Subsystem-Plans-CN.md).
add_library(Atom_Physics STATIC
    Engine/World/Physics/Shapes/Material.hpp
    Engine/World/Physics/Shapes/MassProperties.hpp
    Engine/World/Physics/Shapes/Shape.hpp
    Engine/World/Physics/Shapes/Shape.cpp
    Engine/World/Physics/Dynamics/RigidBody.hpp
    Engine/World/Physics/Dynamics/RigidBody.cpp
    Engine/World/Physics/Collision/BroadPhase.hpp
    Engine/World/Physics/Collision/BroadPhase.cpp
    Engine/World/Physics/Collision/Contact.hpp
    Engine/World/Physics/Collision/NarrowPhase.hpp
    Engine/World/Physics/Collision/NarrowPhase.cpp
    Engine/World/Physics/Solver/ContactSolver.hpp
    Engine/World/Physics/Solver/ContactSolver.cpp
    Engine/World/Physics/Solver/RevoluteJoint.hpp
    Engine/World/Physics/Solver/RevoluteJoint.cpp
    Engine/World/Physics/PhysicsWorld/FixedStepper.hpp
    Engine/World/Physics/Contracts/PhysicsTypes.hpp
    Engine/World/Physics/Contracts/IPhysicsBody.hpp
    Engine/World/Physics/Contracts/IPhysicsWorld.hpp
    Engine/World/Physics/Contracts/IPhysicsJoint.hpp
    Engine/World/Physics/Contracts/IPhysicsBackend.hpp
    Engine/World/Physics/Contracts/PhysicsBackendId.hpp
    Engine/World/Physics/Backend/Internal/InternalPhysicsBody.hpp
    Engine/World/Physics/Backend/Internal/InternalPhysicsBody.cpp
    Engine/World/Physics/Backend/Internal/InternalPhysicsWorld.hpp
    Engine/World/Physics/Backend/Internal/InternalPhysicsWorld.cpp
    Engine/World/Physics/Backend/Internal/InternalRevoluteJoint.hpp
    Engine/World/Physics/Backend/Internal/InternalRevoluteJoint.cpp
    Engine/World/Physics/Backend/Internal/InternalPhysicsBackend.hpp
    Engine/World/Physics/Backend/Internal/InternalPhysicsBackend.cpp
)
target_include_directories(Atom_Physics PUBLIC "${PROJECT_SOURCE_DIR}/Engine/World")
# Physics can fork pure narrow-phase batches through the engine scheduler.
# The scheduler remains an implementation detail: IPhysics* headers do not
# expose JobSystem types and callers retain a serial fallback when it is off.
target_link_libraries(Atom_Physics PUBLIC Atom_Algorithm Atom_Error Atom_Log)
target_link_libraries(Atom_Physics PRIVATE Atom_Concurrency)

# Physics integration — the only code that knows both an ECS world and a physics
#     world, and therefore the only place that converts units or translates handles.
#     Depends on Atom_Physics + Atom_ECS and deliberately NOT on Atom_Runtime: contacts are
#     returned as values, so the caller decides whether they become GameEvents.
add_library(Atom_PhysicsIntegration STATIC
    Engine/World/Physics/Integration/Physics2DWorld.hpp
    Engine/World/Physics/Integration/Physics2DWorld.cpp
)
target_include_directories(Atom_PhysicsIntegration PUBLIC "${PROJECT_SOURCE_DIR}/Engine/World")
target_link_libraries(Atom_PhysicsIntegration PUBLIC Atom_Physics Atom_ECS Atom_Log)

# Scene — the level document format (T2.1) and its loader (T2.2).
#     Depends on ECS + FS + Json and NOT on Runtime: a loader is usable by a tool or
#     a test with no frame loop, and the switch is a single
#     GameWorld::ReplaceEntities call made by the game code.
add_library(Atom_Scene STATIC
    Engine/World/Scene/SceneTypes.hpp
    Engine/World/Scene/SceneSchema.hpp
    Engine/World/Scene/SceneSchema.cpp
    Engine/World/Scene/SceneLoader.hpp
    Engine/World/Scene/SceneLoader.cpp
)
target_include_directories(Atom_Scene PUBLIC "${PROJECT_SOURCE_DIR}/Engine/World")
# The schema and the loader are public; the JSON DOM stays behind SceneSchema, and
# nothing in ECS, Render or Runtime may name it.
target_link_libraries(Atom_Scene PUBLIC Atom_ECS Atom_FS Atom_Json Atom_Log Atom_Math Atom_Color
    Atom_Sprite)

# Reusable 2D world systems. Data is chunk-addressed and gameplay-neutral;
#     applications decide how placed objects and animation events become rules.
add_library(Atom_World2D STATIC
    Engine/World/World2D/ObjectLayer2D.hpp
    Engine/World/World2D/ObjectLayer2D.cpp
    Engine/World/World2D/Tilemap.hpp
    Engine/World/World2D/Tilemap.cpp
    Engine/World/World2D/TilemapLoader.hpp
    Engine/World/World2D/TilemapLoader.cpp
    Engine/World/World2D/TilemapPhysics2D.hpp
    Engine/World/World2D/TilemapPhysics2D.cpp
    Engine/World/World2D/Animation2D.hpp
    Engine/World/World2D/Animation2D.cpp
    Engine/World/World2D/CameraSystem2D.hpp
    Engine/World/World2D/CameraSystem2D.cpp
    Engine/World/World2D/ParticleSystem2D.hpp
    Engine/World/World2D/ParticleSystem2D.cpp
    Engine/World/World2D/World2DRenderer.hpp
    Engine/World/World2D/World2DRenderer.cpp
)
target_include_directories(Atom_World2D PUBLIC "${PROJECT_SOURCE_DIR}/Engine/World")
target_link_libraries(Atom_World2D PUBLIC Atom_ECS Atom_Math Atom_Color Atom_Scene2D Atom_FS Atom_Json Atom_Random)
