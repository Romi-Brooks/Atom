# Atom Physics

`Physics` owns simulation state and collision workflow. It may depend on
`Algorithm/Math` and `Algorithm/Geometry`; those lower modules must never
depend on `Physics`.

Design and roadmap: [`Docs/Physics-Plan-CN.md`](../Docs/Physics-Plan-CN.md).

## Boundary

- `Algorithm/Geometry`: immutable value types and pure queries such as point
  containment, distance, and intersection between two known primitives.
- `Physics`: colliders, rigid bodies, broad phase, narrow phase, contact
  manifolds, integration, constraints, and solvers.
- `Render`: visualizes a world but never supplies simulation state.
- `ECS`: owns entity/component storage; Physics owns the systems that operate
  on its physics components (independent world + thin component shell).

## Shape

```text
Physics/
  Shapes/       // Shape (Circle/ConvexPolygon/Segment), Material, mass properties  [M1 done]
  Dynamics/     // Body state, forces, integration                                   [M2]
  Collision/    // Broad phase, narrow phase, contact manifolds                      [M2/M3]
  Solver/       // Impulses and constraints                                          [M4/M6]
  PhysicsWorld/ // Lifetime, fixed-step orchestration, queries, events, sleeping     [M5]

Backend/              // physics backends live in the shared Backend/ tree
  Contracts/Physics/  // IPhysicsBody / IPhysicsWorld / IPhysicsBackend contract   [M1 done]
  Physics/Internal/   // self-developed Internal backend (Box2D is a future peer)
```

Units: meters (MKS), y-up, radians internally; the game layer converts from
render pixels (y-down) via PPM. The public contract never exposes third-party
types; a Box2D backend (future) stays hidden behind `IPhysicsBackend`.
