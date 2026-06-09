# Engine Refactoring Proposals

This document collects architectural refactoring suggestions for future consideration.

---

## 1. Entity System Redesign

### Current State

Simple class inheritance: `Entity` → `Player`, `Entity` → `NPC`.

Problems:
- Data and behavior are mixed
- Hard to add new entity types without modifying existing code
- No support for shared resource management (textures)
- Move assignment operator is incomplete

### Proposed: Component-Based Architecture (Phase 1)

```cpp
// Entity = ID + component container
class Entity {
    uint64_t id_;
    std::unordered_map<ComponentType, std::unique_ptr<Component>> components_;
public:
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args);

    template<typename T>
    T* GetComponent();

    template<typename T>
    void RemoveComponent();
};

// Components = pure data structs
struct TransformComponent {
    Vec2 position;
    Vec2 scale{1, 1};
    float rotation{0};
};

struct HealthComponent {
    float current;
    float max;
};

struct RenderableComponent {
    sf::Sprite sprite;
    int z_order{0};
};

struct PhysicsComponent {
    Vec2 velocity;
    Vec2 acceleration;
    float mass{1};
    bool is_static{false};
};

// Systems = pure behavior, operate on entities with specific components
class MovementSystem {
public:
    void Update(EntityManager& manager, float dt) {
        for (auto& entity : manager.GetEntitiesWith<TransformComponent, PhysicsComponent>()) {
            auto& transform = entity.GetComponent<TransformComponent>();
            auto& physics = entity.GetComponent<PhysicsComponent>();

            physics.velocity += physics.acceleration * dt;
            transform.position += physics.velocity * dt;
        }
    }
};
```

### Long-Term Goal: Full ECS (Phase 2)

Reference implementation: [EnTT](https://github.com/skypjack/entt).

Key differences from Phase 1:
- Components stored in contiguous arrays (cache-friendly)
- Entities are just integer IDs
- Systems query component pools directly (no per-entity virtual calls)

### Migration Path

```
Current (inheritance) → Phase 1 (composition, 1-2 days) → Phase 2 (ECS, long-term)
```

---

## 2. Atom.cpp Positioning

### Current

`Atom.cpp` is a minimal entry point that prints a banner and logs one message. It serves as a compile smoke test.

### Recommendation

Keep it as a compile smoke test, but add explicit comments to clarify its purpose:

```cpp
// Atom.cpp — Compile Smoke Test for Atom Engine
// This file verifies that all engine modules link correctly.
// It does NOT serve as an application entry point.
// Game projects should create their own main() and link against engine_core.
```

Alternative: Wrap in `#ifdef ATOM_STANDALONE` to allow it to be a runnable demo when needed.

---

## 3. Third-Party Dependency Management

### Current

All 7 third-party libraries are expected under `ThirdParty/Lib/` with manual setup.

### Proposed: Git Submodules + Setup Script

```
ThirdParty/
├── Lib/                    # gitignored, downloaded by setup script
│   ├── SFML-3.0.0/
│   ├── FFmpeg/
│   └── taglib/
└── Source/                 # Git submodules
    ├── imgui/              → gh:ocornut/imgui
    ├── imgui-sfml/         → gh:eliasdaler/imgui-sfml
    ├── lua/                → gh:lua/lua
    └── utfcpp/             → gh:nemtrif/utfcpp
```

### Submodules for source libs, setup script for binary libs

```powershell
# setup_deps.ps1 (Windows)
git submodule update --init --recursive
# Download + extract pre-built SFML, FFmpeg, TagLib binaries
```

```bash
# User setup flow:
git clone --recursive https://github.com/your/repo.git
cd repo
./setup_deps.ps1
```

### Alternative: vcpkg Manifest

```json
// vcpkg.json
{
    "name": "atom-engine",
    "version": "0.1.0",
    "dependencies": [
        "sfml",
        "imgui",
        "imgui-sfml",
        "lua",
        "taglib",
        "utfcpp",
        "ffmpeg"
    ]
}
```

Pro: Fully automated.
Con: Requires vcpkg installation, may lag on SFML 3.0 support.
