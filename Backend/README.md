# Atom Backend Layer

`Backend` defines the boundary between Atom's engine code and concrete platform or multimedia implementations.

## Layout

- `Contracts/` contains backend-independent contracts and shared data types required by Atom.
- `Registry/` stores backend factories and the audio decoder registry (extension → decoder factory).
- `Runtime/` owns global backend selection, default registration, and audio backend switching; it also registers the engine's default format decoders (`.wav` → WavRiffDecoder, `.mp3` → minimp3 decoder).
- `Builtin/` contains Atom-owned implementations such as the WAV RIFF decoder (`WavRiffDecoder`) and the minimp3-based MP3 decoder (`Minimp3Decoder`).
- `SDL3/` contains SDL lifecycle, window, render, audio playback, and SDL IO implementations.
- Future platform backends should use sibling directories such as `Null/` or `OpenAL/`. Codec integrations belong under an appropriate decoder implementation rather than being treated as a complete platform backend.

## Dependency rule

Atom domain modules may depend on `Backend/Contracts`, `Backend/Registry`, and the composition API in `Backend/Runtime`, but they should not depend directly on a concrete backend. Concrete backends implement the contracts and are selected by the runtime composition layer.

Audio players and decoders already use registry/runtime composition. The window facade still owns an `SDL3RenderWindow` directly; further separation of window management and rendering is tracked by `RENDER-001` in [`Docs/Remaining-Issues.md`](../Docs/Remaining-Issues.md).
