# Atom Physics

`Physics` owns simulation state and collision workflow. It may depend on
`Algorithm/Math` and `Algorithm/Geometry`; those lower modules must never
depend on `Physics`.

## Boundary

- `Algorithm/Geometry`: immutable value types and pure queries such as point
  containment, distance, and intersection between two known primitives.
- `Physics`: colliders, rigid bodies, broad phase, narrow phase, contact
  manifolds, integration, constraints, and solvers.
- `Render`: visualizes a world but never supplies simulation state.
- `ECS`: will own entity/component storage; Physics owns the systems that
  operate on its physics components.

## Planned shape

```text
Physics/
  Shapes/       // Collider shape descriptions and material properties
  Dynamics/     // Body state, forces, integration
  Collision/    // Broad phase, narrow phase, contact manifolds
  Solver/       // Impulses and constraints
  PhysicsWorld/ // Lifetime, fixed-step orchestration, queries
```