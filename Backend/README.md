# Atom Backend Layer

`Backend` defines the boundary between Atom's engine code and concrete platform or multimedia implementations.

## Layout

- `Contracts/` contains backend-independent contracts and shared data types required by Atom.
- `SDL3/` contains the SDL3 implementation, separated into runtime, window, render, and audio responsibilities.
- Future implementations should use sibling directories such as `SFML/`, `FFmpeg/`, or `Null/`.

## Dependency rule

Atom domain modules may depend on `Backend/Contracts`, but they should not depend directly on a concrete backend. Concrete backends implement the contracts and are selected by the application composition layer.

The current codebase still has several direct SDL3 constructions in domain modules. These are tracked by `ARCH-003` and will be removed through backend factories and dependency injection.
