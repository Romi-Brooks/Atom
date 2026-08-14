# Atom Backend Layer

`Backend` defines the boundary between Atom's engine code and concrete platform or multimedia implementations.

## Layout

- `Contracts/` contains backend-independent contracts and shared data types required by Atom.
- `Registry/` stores backend and audio decoder factories.
- `Runtime/` owns global backend selection, default registration, and audio backend switching.
- `Builtin/` contains Atom-owned implementations such as the experimental WAV RIFF decoder.
- `SDL3/` contains SDL lifecycle, window, render, audio playback, and SDL decoder implementations.
- Future platform backends should use sibling directories such as `Null/` or `OpenAL/`. Codec integrations belong under an appropriate decoder implementation rather than being treated as a complete platform backend.

## Dependency rule

Atom domain modules may depend on `Backend/Contracts`, `Backend/Registry`, and the composition API in `Backend/Runtime`, but they should not depend directly on a concrete backend. Concrete backends implement the contracts and are selected by the runtime composition layer.

Audio players and decoders already use registry/runtime composition. The window facade still owns an `SDL3RenderWindow` directly; further separation of window management and rendering is tracked by `RENDER-001` in [`Docs/Remaining-Issues.md`](../Docs/Remaining-Issues.md).
