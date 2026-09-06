# Algorithm Module — Specification

This document lists the pure algorithms, theorems, and value types owned by
Atom's `Algorithm` module. It is organized into **Math**, **Geometry**,
**Interpolation**, and **Utility** categories. Physics simulation and collision
workflow live in [`Physics/`](../Physics/README.md).

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
| 1.3.1 | **AABB / Bounds** | `Aabb2` min/max representation with size, center, point containment and overlap. | 🟡 Core done |
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
| 1.4.6 | **Easing Functions** | Sine, cubic and back curves are available; additional curve families remain. | 🟡 Core done |

### 1.5 Transformations

| # | Name | Description | Status |
|---|------|-------------|--------|
| 1.5.1 | **Transform2D** | Position/rotation/scale/pivot composition to Mat3; hierarchy remains a scene-layer concern. | ✅ Core done |
| 1.5.2 | **Coordinate Conversion** | Camera2D world ↔ screen conversion is available; UI scaling policy remains. | 🟡 Core done |
| 1.5.3 | **Transform3D** | Position/Euler rotation/scale composition to Mat4. | ✅ Core done |
| 1.5.4 | **Camera2D / Camera3D** | 2D world/screen conversion and 3D view/projection composition. | ✅ Core done |

---

## 2. Utility Algorithms

| # | Name | Description | Status |
|---|------|-------------|--------|
| 2.1 | **Noise (Perlin / Simplex)** | 1D/2D/3D coherent noise for procedural generation. | ❌ TODO |
| 2.2 | **Delaunay Triangulation** | Triangulate a set of points. Useful for procedural mesh generation, AI navigation meshes. | ❌ TODO |
| 2.3 | **A* Pathfinding** | A* search on a 2D grid/navmesh with heuristic (Manhattan, Euclidean). | ❌ TODO |
| 2.4 | **BFS / DFS** | Graph traversal for simple AI, connectivity testing. | ❌ TODO |
| 2.5 | **Dijkstra** | Weighted shortest path (fallback when heuristic is unreliable). | ❌ TODO |
| 2.6 | **KD-Tree** | K-dimensional tree for spatial queries (nearest neighbor). | ❌ TODO |
| 2.7 | **Frustum Culling** | Test if AABB/circle is within the viewport. | ❌ TODO |

---

## Implementation Priority

```
P0 (Core Foundation - Do first):
  ├── Mat3x3 (Transform2D depends on it)
  ├── Rect
  ├── Circle
  ├── Lerp / Inverse Lerp / SmoothStep
  ├── Transform2D
  ├── Rectangle / bounds naming contract
  └── Color values and interpolation

P1 (Geometry and utility algorithms - Do second):
  ├── Polygon
  ├── Easing curve families
  ├── Frustum culling
  └── Spatial query structures

P2 (Advanced Features - Do third):
  ├── A* Pathfinding
  ├── Noise (Perlin / Simplex)
  ├── Bezier / Catmull-Rom Splines
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
- Collision results, contacts, spatial broad phase, and solvers are specified by `Physics`, not this module.
