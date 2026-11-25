# Interfaces (header-only, no SDL3 dependency)
add_library(Atom_Backend_Contracts INTERFACE)
target_sources(Atom_Backend_Contracts INTERFACE
    Engine/Atom/Backend/Contracts/Audio/AudioTypes.hpp
    Engine/Atom/Backend/Contracts/Audio/AudioExtensions.hpp
    Engine/Atom/Backend/Contracts/Audio/AudioBackendId.hpp
    Engine/Atom/Backend/Contracts/Audio/AudioSourceRegistry.hpp
    Engine/Atom/Backend/Contracts/Audio/IAudioBackend.hpp
    Engine/Atom/Backend/Contracts/Audio/IAudioBackendChangeListener.hpp
    Engine/Atom/Backend/Contracts/Audio/IAudioDecoder.hpp
    Engine/Atom/Backend/Contracts/Audio/IAudioSource.hpp
    Engine/Atom/Backend/Contracts/Audio/NullAudioBackend.hpp
    Engine/Atom/Backend/Contracts/Debug/IDebugImGuiBackend.hpp
    Engine/Atom/Backend/Contracts/Numeric/NumericContract.hpp
    Engine/Atom/Backend/Contracts/Render/IRender2DContext.hpp
    Engine/Atom/Backend/Contracts/Render/IRenderBackend.hpp
    Engine/Atom/Backend/Contracts/Render/IRenderDevice.hpp
    Engine/Atom/Backend/Contracts/Render/IRenderTarget.hpp
    Engine/Atom/Backend/Contracts/Render/IRenderTypes.hpp
    Engine/Atom/Backend/Contracts/Render/ITexture.hpp
    Engine/Atom/Backend/Contracts/Render/PresentMode.hpp
    Engine/Atom/Backend/Contracts/Render/RenderBackendConfig.hpp
    Engine/Atom/Backend/Contracts/Render/RenderBackendId.hpp
    Engine/Atom/Backend/Contracts/Time/ITimeSource.hpp
    Engine/Atom/Backend/Contracts/Video/IVideoDecoder.hpp
    Engine/Atom/Backend/Contracts/Window/IWindow.hpp
    Engine/Atom/Backend/Contracts/Window/WindowMode.hpp
)
target_include_directories(Atom_Backend_Contracts INTERFACE
    "${PROJECT_SOURCE_DIR}/Engine"
)
# A0.2: the contracts' public headers include Engine/Atom/Core/Algorithm/Vector/Vec2.hpp,
# Engine/Atom/Core/Algorithm/Color/Color.hpp and Engine/Atom/Input/Key.hpp, and Audio's decoder contract
# includes Engine/Atom/Resources/Filesystem/FileSystem.hpp for IFile. Every one of those is now a
# declared dependency instead of an include path that merely happens to
# resolve in a single-tree checkout. Atom_FS is attached further down, next
# to the Atom_FS definition, so no target is referenced before it exists.
target_link_libraries(Atom_Backend_Contracts INTERFACE Atom_Math Atom_Color Atom_Input Atom_FS)

# SDL3 Core
add_library(Atom_Backend_SDL3_Core ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Backend/SDL3/Core/SDLRuntime.cpp
    Engine/Atom/Backend/SDL3/Core/SDLRuntime.hpp
    Engine/Atom/Backend/SDL3/Core/SdlError.cpp
    Engine/Atom/Backend/SDL3/Core/SdlError.hpp
    Engine/Atom/Backend/SDL3/Time/SDL3TimeSource.cpp
    Engine/Atom/Backend/SDL3/Time/SDL3TimeSource.hpp
)
target_include_directories(Atom_Backend_SDL3_Core PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
# SDL3TimeSource implements backend::ITimeSource, so the contract library is
# a real dependency here, not just an include path that happens to resolve.
target_link_libraries(Atom_Backend_SDL3_Core PUBLIC Atom_3rd_SDL3 Atom_Backend_Contracts)

# SDL3 window platform layer
add_library(Atom_Backend_SDL3_Window ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Backend/SDL3/Window/SDL3Window.cpp
    Engine/Atom/Backend/SDL3/Window/SDL3Window.hpp
)
target_include_directories(Atom_Backend_SDL3_Window PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Backend_SDL3_Window PUBLIC Atom_Backend_Contracts Atom_Backend_SDL3_Core Atom_Algorithm Atom_Log)

# SDL_GPU render device and backend composition.
add_library(Atom_Backend_SDL_GPU ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Backend/SDLGPU/Device/SDLGPUDevice.cpp
    Engine/Atom/Backend/SDLGPU/Device/SDLGPUDevice.hpp
    Engine/Atom/Backend/SDLGPU/Device/SDLGPUBackend.cpp
    Engine/Atom/Backend/SDLGPU/Device/SDLGPUBackend.hpp
    Engine/Atom/Backend/SDLGPU/Encoding/SDLGPU2D.cpp
    Engine/Atom/Backend/SDLGPU/Pipelines/SDLGPU2DPipelineFactory.cpp
    Engine/Atom/Backend/SDLGPU/Pipelines/SDLGPU2DPipelineFactory.hpp
    Engine/Atom/Backend/SDLGPU/Pipelines/SDLGPUShaderLoader.cpp
    Engine/Atom/Backend/SDLGPU/Pipelines/SDLGPUShaderLoader.hpp
)
target_include_directories(Atom_Backend_SDL_GPU PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Backend_SDL_GPU PUBLIC Atom_Backend_Contracts Atom_Backend_SDL3_Window Atom_Log Atom_3rd_SDL3)
target_compile_definitions(Atom_Backend_SDL_GPU PRIVATE
    ATOM_SHADER_HAS_DXIL=$<BOOL:${ATOM_SHADER_BUILD_DXIL}>
    ATOM_SHADER_HAS_MSL=$<BOOL:${ATOM_SHADER_BUILD_MSL}>)

# SDL_GPU Debug (ImGui) Backend — ImGui glue for the SDL_GPU render backend.
add_library(Atom_Backend_SDLGPU_Debug ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Backend/SDLGPU/Debug/SDLGPUImGuiBackend.cpp
    Engine/Atom/Backend/SDLGPU/Debug/SDLGPUImGuiBackend.hpp
)
target_include_directories(Atom_Backend_SDLGPU_Debug PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Backend_SDLGPU_Debug PUBLIC
    Atom_Backend_Contracts Atom_Backend_SDL_GPU Atom_3rd_ImGui_SDL3)

# Render Backend Runtime — the sanctioned layer that knows the concrete
#     backends and registers them (window + render-device factory);
#     upper layers only consume the registries and the I* interfaces.
add_library(Atom_Backend_Render_Composition ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Backend/Composition/RenderBackendRuntime.cpp
    Engine/Atom/Backend/Composition/RenderBackendRuntime.hpp
)
target_include_directories(Atom_Backend_Render_Composition PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Backend_Render_Composition PUBLIC Atom_Backend_Extension Atom_Backend_SDL_GPU)
target_link_libraries(Atom_Backend_Render_Composition PRIVATE Atom_Backend_SDLGPU_Debug)

# Compatibility only; new code should name the composition role directly.
add_library(Atom_Backend_Render_Runtime INTERFACE)
target_link_libraries(Atom_Backend_Render_Runtime INTERFACE Atom_Backend_Render_Composition)

# Backend extension registration and format codecs
add_library(Atom_Backend_Extension ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Resources/Media/Audio/Resources/AudioDecoderRegistry.hpp
    Engine/Atom/Backend/Extension/AudioDecoderRegistry.cpp
    Engine/Atom/Backend/Extension/BackendRegistry.hpp
    Engine/Atom/Backend/Extension/BackendRegistry.cpp
    Engine/Atom/Backend/Extension/RenderBackendRegistry.hpp
    Engine/Atom/Backend/Extension/RenderBackendRegistry.cpp
    Engine/Atom/Backend/Extension/DebugImGuiBackendRegistry.hpp
    Engine/Atom/Backend/Extension/DebugImGuiBackendRegistry.cpp
)
target_include_directories(Atom_Backend_Extension PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Backend_Extension PUBLIC Atom_Backend_Contracts)

# SDL3 Audio Backend
add_library(Atom_Backend_SDL3_Audio ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Backend/SDL3/Audio/SDL3AudioBackend.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3AudioBackend.hpp
    Engine/Atom/Backend/SDL3/Audio/SDL3SFXSource.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3SFXSource.hpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MusicSource.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MusicSource.hpp
    Engine/Atom/Backend/SDL3/Audio/SDL3StreamingMusicSource.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3StreamingMusicSource.hpp
    Engine/Atom/Backend/SDL3/Audio/SDL3WavDecoder.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3WavDecoder.hpp
)
target_include_directories(Atom_Backend_SDL3_Audio PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Backend_SDL3_Audio PUBLIC Atom_Backend_Contracts Atom_Backend_Extension Atom_Backend_SDL3_Core Atom_Log)

add_library(Atom_Backend_SDL3_Mixer_Audio ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Backend/SDL3/Audio/SDL3MixerAudioBackend.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MixerAudioBackend.hpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MixerContext.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MixerContext.hpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MixerSource.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MixerSource.hpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MixerStreamingSource.cpp
    Engine/Atom/Backend/SDL3/Audio/SDL3MixerStreamingSource.hpp
)
target_include_directories(Atom_Backend_SDL3_Mixer_Audio PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Backend_SDL3_Mixer_Audio PUBLIC Atom_Backend_Contracts Atom_Backend_SDL3_Core Atom_Log Atom_3rd_SDL3_mixer)

add_library(Atom_Backend_Composition ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Backend/Composition/BackendRuntime.hpp
    Engine/Atom/Backend/Composition/BackendRuntime.cpp
)
target_include_directories(Atom_Backend_Composition PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Backend_Composition PUBLIC
    Atom_Backend_Contracts
    Atom_Backend_Extension
    Atom_Audio_Codecs
    Atom_Backend_SDL3_Audio
    Atom_Backend_SDL3_Mixer_Audio)

# Compatibility only; BackendRuntime is a composition service, not the
# game Runtime module.
add_library(Atom_Backend_Runtime INTERFACE)
target_link_libraries(Atom_Backend_Runtime INTERFACE Atom_Backend_Composition)
