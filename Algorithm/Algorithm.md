# Algorithm Module — Specification

This document lists the algorithms, theorems, and data structures that the Atom Engine's `Algorithm` module should implement. The module is organized into **Math**, **Physics/Collision**, and **Utility** categories.

---

## 1. Core Math

### 1.1 Vectors

| # | Name | Description | Status |
|---|------|-------------|--------|
| 1.1.1 | **Vec2** | 2D vector (x, y). Addition, subtraction, dot product, cross product (scalar), normalization, length, length squared, lerp, reflection, rotation by angle. | ✅ Done |
| 1.1.2 | **Vec3** | 3D vector arithmetic, dot/cross, normalization, distance and interpolation. | ✅ Done |
| 1.1.3 | **Vec4** | 4D vector arithmetic for homogeneous coordinates and shader data. | ✅ Done |

### 1.2 Matrices

| # | Name | Description | Status |
|---|------|-------------|--------|
| 1.2.1 | **Mat3x3** | Column-major 3×3 matrix for 2D translation, rotation, scale, composition and inversion. | ✅ Core done |
| 1.2.2 | **Mat4x4** | Column-major 4×4 matrix with transforms, look-at and SDL_GPU-compatible projections. | ✅ Core done |

### 1.3 Geometry Primitives

| # | Name | Description | Status |
|---|------|-------------|--------|
| 1.3.1 | **Rect** | `Rect2` min/max representation with size, center, point containment and overlap. | 🟡 Core done |
| 1.3.2 | **Circle** | `Circle2` with point containment and circle overlap. | 🟡 Core done |
| 1.3.3 | **Line / Ray** | `Ray3` origin/direction and point evaluation; intersection tests remain. | 🟡 Basic done |
| 1.3.4 | **Polygon** | Convex/concave polygon (vertex array). Area (shoelace formula), centroid, contains point (ray casting), triangulation (ear clipping). | ❌ TODO |
| 1.3.5 | **AABB** / **OBB** | `AABB3` containment/overlap is available; OBB remains. | 🟡 AABB done |

### 1.4 Interpolation & Curves

| # | Name | Description | Status |
|---|------|-------------|--------|
| 1.4.1 | **Lerp** | Linear interpolation for scalar, Vec2 and Vec3. | ✅ Done |
| 1.4.2 | **Inverse Lerp** | Safe scalar inverse interpolation. | ✅ Done |
| 1.4.3 | **SmoothStep** | Clamped Hermite interpolation. | ✅ Done |
| 1.4.4 | **Bezier Curve** | Quadratic and cubic Bézier curves. Point evaluation, tangent at t. | ❌ TODO |
| 1.4.5 | **Catmull-Rom Spline** | Spline through control points. Useful for camera paths, animation curves. | ❌ TODO |
| 1.4.6 | **Easing Functions** | Common easing curves (ease-in, ease-out, ease-in-out) for sine, quad, cubic, quart, quint, expo, elastic, bounce, back. | ❌ TODO |

### 1.5 Transformations

| # | Name | Description | Status |
|---|------|-------------|--------|
| 1.5.1 | **Transform2D** | Position/rotation/scale/pivot composition to Mat3; hierarchy remains a scene-layer concern. | ✅ Core done |
| 1.5.2 | **Coordinate Conversion** | Camera2D world ↔ screen conversion is available; UI scaling policy remains. | 🟡 Core done |
| 1.5.3 | **Transform3D** | Position/Euler rotation/scale composition to Mat4. | ✅ Core done |
| 1.5.4 | **Camera2D / Camera3D** | 2D world/screen conversion and 3D view/projection composition. | ✅ Core done |

---

## 2. Collision Detection

| # | Name | Description | Status |
|---|------|-------------|--------|
| 2.1 | **AABB vs AABB** | Overlap test for axis-aligned rectangles. | ❌ TODO |
| 2.2 | **Circle vs Circle** | Distance-based overlap test. | ❌ TODO |
| 2.3 | **Circle vs AABB** | Circle vs rectangle collision (closest point on rect to circle center). | ❌ TODO |
| 2.4 | **Line vs AABB** | Liang-Barsky or Slab method. | ❌ TODO |
| 2.5 | **Line vs Circle** | Ray-circle intersection. | ❌ TODO |
| 2.6 | **Point in Polygon** | Ray casting algorithm. | ❌ TODO |
| 2.7 | **Convex Polygon vs Convex Polygon** | Separating Axis Theorem (SAT). | ❌ TODO |
| 2.8 | **SAT with Circle** | Expanding SAT for circle vs polygon. | ❌ TODO |
| 2.9 | **Spatial Hashing** | Grid-based broad-phase collision culling. | ❌ TODO |
| 2.10 | **Quadtree** | Spatial partitioning for 2D broad-phase culling. | ❌ TODO |
| 2.11 | **Contact Manifold** | Generate collision contact points (position, normal, penetration depth). | ❌ TODO |

---

## 3. Physics

| # | Name | Description | Status |
|---|------|-------------|--------|
| 3.1 | **Rigid Body** | Position, velocity, acceleration, mass, moment of inertia, restitution (bounciness), friction. | ❌ TODO |
| 3.2 | **Integration** | Euler, Semi-implicit Euler (Symplectic), Verlet integration for `F = ma`. | ❌ TODO |
| 3.3 | **Impulse Resolution** | Collision response via impulse. Relative velocity, normal impulse, friction impulse. | ❌ TODO |
| 3.4 | **Constraint** | Position constraint, distance constraint (rod/spring). | ❌ TODO |
| 3.5 | **Spring-Damper** | Hooke's law + damping: `F = -k*x - c*v`. | ❌ TODO |
| 3.6 | **Gravity** | Constant gravity force. | ❌ TODO |
| 3.7 | **Drag / Air Resistance** | Velocity-proportional drag: `F = -b*v`. | ❌ TODO |

---

## 4. Utility Algorithms

| # | Name | Description | Status |
|---|------|-------------|--------|
| 4.1 | **Noise (Perlin / Simplex)** | 1D/2D/3D coherent noise for procedural generation. | ❌ TODO |
| 4.2 | **Delaunay Triangulation** | Triangulate a set of points. Useful for procedural mesh generation, AI navigation meshes. | ❌ TODO |
| 4.3 | **A* Pathfinding** | A* search on a 2D grid/navmesh with heuristic (Manhattan, Euclidean). | ❌ TODO |
| 4.4 | **BFS / DFS** | Graph traversal for simple AI, connectivity testing. | ❌ TODO |
| 4.5 | **Dijkstra** | Weighted shortest path (fallback when heuristic is unreliable). | ❌ TODO |
| 4.6 | **KD-Tree** | K-dimensional tree for spatial queries (nearest neighbor). | ❌ TODO |
| 4.7 | **Frustum Culling** | Test if AABB/circle is within the viewport. | ❌ TODO |

---

## 5. Color & Image

| # | Name | Description | Status |
|---|------|-------------|--------|
| 5.1 | **Color** | RGBA color with float or byte components. Blending (alpha, additive, multiply), HSL/HSV ⇔ RGB conversion, hex parsing. | ❌ TODO |
| 5.2 | **Color Grading** | Lerp between colors, brightness/contrast adjustment, gamma correction. | ❌ TODO |

---

## Implementation Priority

```
P0 (Core Foundation - Do first):
  ├── Mat3x3 (Transform2D depends on it)
  ├── Rect
  ├── Circle
  ├── Lerp / Inverse Lerp / SmoothStep
  ├── Transform2D
  ├── AABB vs AABB
  ├── Circle vs Circle
  ├── Color
  └── Rigid Body + Integration

P1 (Collision & Physics - Do second):
  ├── SAT (Separating Axis Theorem)
  ├── Contact Manifold
  ├── Impulse Resolution
  ├── Spatial Hashing / Quadtree
  ├── Polygon
  └── Easing Functions

P2 (Advanced Features - Do third):
  ├── A* Pathfinding
  ├── Noise (Perlin / Simplex)
  ├── Bezier / Catmull-Rom Splines
  ├── Constraint / Spring-Damper
  ├── Delaunay Triangulation
  └── KD-Tree
```

---

## Notes

- Matrix storage is column-major and matrices multiply column vectors.
- The 3D convention is left-handed with normalized device depth `[0, 1]`, matching SDL_GPU.
- Angles passed to math and transform APIs are radians; use `ToRadians`/`ToDegrees` at boundaries.

- All algorithms should be **generic** (templates where appropriate) and **header-only** where possible.
- Use SIMD intrinsics (SSE/NEON) for hot paths: Mat3x3×Vec2, AABB overlap, color blending.
- Vector types should support both `float` and `double` precision via template parameter.
- Collision functions should return a `CollisionResult` struct containing `bool hit`, `Vec2 point`, `Vec2 normal`, `float depth`.
