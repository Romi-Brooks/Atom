# Atom Backend Layer

`Backend` defines the boundary between Atom's engine code and concrete platform or multimedia implementations.

## Layout

- `Contracts/` contains backend-independent contracts and shared data types required by Atom.
- `Extension/` stores the explicitly opt-in backend factories and audio decoder registry (extension → decoder factory).
- `Runtime/` owns global backend selection, default registration, and audio backend switching; it also registers the engine's default format decoders (`.wav` → WavProfDecoder, `.mp3` → minimp3 decoder).
- `Audio/` contains backend-independent audio adapters such as the WAV decoder (`WavProfDecoder` / RiffWaveReader) and the minimp3-based MP3 decoder (`Minimp3Decoder`).
- `SDL3/` contains SDL lifecycle, platform-window, audio playback, and SDL IO implementations.
- `SDLGPU/` contains the SDL_GPU render device, backend composition, Renderer2D context, ImGui adapter, and SDL_GPU-specific pipeline/resource code.

`SDL3/` and `SDLGPU/` are intentionally separate layers, not duplicate render
backends. `SDL3/Core` owns SDL process lifetime, `SDL3/Window` owns windows and
events, and `SDL3/Audio` owns SDL audio playback. `SDLGPU` consumes an SDL3
window but owns the SDL_GPU device, swapchain, render passes, pipelines and
GPU-specific ImGui glue. A future native Vulkan implementation belongs in a
separate `Backend/Vulkan/` sibling and must implement the same contracts.

## Dependency rule

Atom domain modules may depend on `Backend/Contracts` and the explicitly opt-in registration API in `Backend/Extension`, but they should not depend on `Backend/Runtime` or a concrete backend. Concrete backends implement the contracts and are selected by the runtime composition layer.

Audio players and decoders use registry/runtime composition. Rendering now follows the same boundary: `RenderWindow` owns an `IRenderBackend`, while `IWindow` handles platform events and `IRenderDevice` handles GPU frames. The default `sdl_gpu` backend combines `SDL3Window` with `SDLGPUDevice`; no SDL renderer is created.
