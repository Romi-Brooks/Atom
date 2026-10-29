# Atom Physics

`Physics` owns simulation state and collision workflow. It may depend on
`Algorithm/Math` and `Algorithm/Geometry`; those lower modules must never
depend on `Physics`.

Design and roadmap: [`../Docs/Plans/Subsystem-Plans-CN.md`](../Docs/Plans/Subsystem-Plans-CN.md).

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
  Contracts/    // IPhysicsBody / IPhysicsWorld / IPhysicsBackend contract           [M1 done]
  Backend/      // self-developed Internal provider; Box2D is a future peer
    Internal/
```

Units: meters (MKS), y-up, radians internally; the game layer converts from
render pixels (y-down) via PPM. The public contract never exposes third-party
types; a Box2D provider (future) stays hidden behind `IPhysicsBackend`. The
directory is owned by Physics, not root `Backend/`; its pending mechanical
rename to `Physics/Backends/` is tracked in `Docs/Remaining-Issues.md`.
