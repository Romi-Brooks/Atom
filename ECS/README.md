# Atom ECS

`ECS` owns entity and component **storage**. It does not own behaviour: systems
live with the subsystem they serve (physics systems in `Physics/`, animation
systems in a future `Animation/`), and they operate on the components this module
stores. That boundary is the one `Physics/README.md` already describes.

## Public surface

| Type | Purpose |
| --- | --- |
| `Entity` | Opaque handle: a 32-bit slot index plus a 32-bit generation |
| `EntityWorld` | Entity lifecycle and typed component access |
| `Component` | Concept: movable, destructible, non-reference |
| components in `Components.hpp` | The engine's component vocabulary, plus the list that drives instantiation |

## The boundary rule

**No third-party type appears in a public header of this module, or anywhere else
in the engine.** The storage backend is an implementation detail of
`EntityWorld.cpp`. That is not tidiness for its own sake; it buys three things:

1. The backend is replaceable. Nothing outside `EntityWorld.cpp` knows what stores
   the components, so a swap is one file plus the component list.
2. `Entity` keeps its own meaning. The backend's default handle packs a 20-bit
   index with a 12-bit version, which wraps after 4096 recycles of one slot and
   would silently resurrect an old handle. This module gives the handle a full
   32-bit generation instead.
3. The Lua binding surface stays engine-owned (ARCH-109): a script deals in
   `Entity`, never in a backend handle.

`target_link_libraries(Atom_ECS PRIVATE Atom_3rd_EnTT)` is what enforces this.
Changing it to `PUBLIC` puts the backend's include directory on the path of every
consumer and turns the rule into a comment.

## Why every operation validates before calling the backend

The backend reports misuse by **asserting**, and its assertions default to
`<cassert>`, which `NDEBUG` erases. Measured behaviour of misuse in an `-O2
-DNDEBUG` build:

| Misuse | With assertions | In a release build |
| --- | --- | --- |
| Destroy an already-destroyed entity | abort | silently "succeeds" |
| Read a component the entity does not own | abort | access violation |
| Insert a component twice | abort | silently overwrites the value |
| Insert a component on a destroyed entity | abort | silently creates it |

That is the failure mode the engine's unified error model exists to remove. So
`EntityWorld` performs the cheap non-asserting checks itself and returns
`atom::Error`:

- `EcsInvalidEntity` - a handle with the reserved index (never issued by this module)
- `EcsEntityNotAlive` - the entity was destroyed, or the handle predates a recycle
- `EcsComponentAlreadyPresent` - `Emplace` where the entity already owns it
- `EcsComponentMissing` - `Get`/`Patch` where the entity does not own it
- `EcsOutOfEntities` - the backend ran out of slots
- `EcsTypeIdCollision` - reserved for a component type whose backend id collides

Iteration order is deliberately **ascending entity index**, not the backend's pool
order: physics and animation want reproducible stepping, and a stable order is what
lets a test assert something about a sequence of updates.

## Build rule: the backend's assertions stay on

`EntityWorld.cpp` does `#undef NDEBUG` before including any backend header. Do not
remove it and do not add `-DNDEBUG` to `Atom_ECS`. The facade's pre-checks are the
first line of defence; the backend's assertions are the second, and they are the
only thing that turns a facade bug into a loud failure instead of memory
corruption. The cost is a few predictable branches per call.

## Adding a component type

1. Define the type in `ECS/Components.hpp` (plain data: no virtuals, no behaviour,
   no back-pointers to the world).
2. Add it to the `ATOM_ECS_COMPONENTS` list in the same file.
3. Add one `ATOM_ECS_INSTANTIATE(T)` line in `EntityWorld.cpp` - or
   `ATOM_ECS_INSTANTIATE_TAG(T)` if it is an empty tag type, which has no value to
   read, write or patch.
4. If a system will iterate it together with others, instantiate that combination
   as well (see the iteration list at the bottom of `EntityWorld.cpp`).

Keeping the lists explicit is what lets the backend stay out of the public headers.
A component that no system iterates costs nothing but its own instantiation.

## Iteration

Two entry points, same query semantics, different cost:

```cpp
// In place. The visitor is a free function so that the loop body can be compiled
// against the real component types without this header naming the backend:
//   world.ForEach<Position, Velocity>({&Integrate::Run, &state});
world.ForEach<Position, Velocity>({&Integrate::Run, &state});

// Materialises a vector of entities instead:
for (const auto entity : world.With<Position, Velocity>()) { ... }
```

`ForEach` is what a hot loop wants: it walks the pools in place. `With` is for
code that needs the list itself (a sweep, a snapshot, a debug panel).

Both visit in **ascending entity index order**, not the backend's pool order. That
is deliberate: the backend's order changes as entities are destroyed, and physics
and animation want reproducible stepping.

Each component arrives as a `std::span` of length 1, so the body reads and writes
it as `positions[0]`. A tag component (empty type) arrives as a **length-0 span**:
presence is the datum, there is nothing to read or write.

A visitor is a function pointer plus a context, which is why the call site looks
like `{&Run, &state}` rather than a lambda. `EntityWorld.hpp` must not name the
backend, so the loop body cannot be a template in the header; the function pointer
is what a backend-free header can carry. The parameters still have their real
types, so there is no `void*` casting at the call site.

Iterating a set that no one has instantiated is a link error, not a silent empty
loop. Add the combination to the iteration list at the bottom of `EntityWorld.cpp`.

**Known limitation:** iteration through a `const EntityWorld&` still yields mutable
component spans, because the backend has no const-aware query. Treat the const
qualifier as covering the entity set, not the component values.

Tags can take part in a query (`ForEach<Active, Position, Velocity>`), which is what
lets a loop skip inactive entities without a per-entity check. They arrive as a
length-0 span.
