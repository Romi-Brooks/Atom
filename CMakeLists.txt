# ============================================================================
# Global Settings
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(Atom LANGUAGES C CXX)

# Target naming convention:
#   Atom_*       - engine libraries, backends, applications, and tools
#   Atom_3rd_*   - Atom-owned wrappers around third-party dependencies
#   Example_*    - runnable examples and smoke programs
# Upstream dependency targets remain confined to ThirdParty/CMakeLists.txt.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# ============================================================================
# Third-party dependencies
# ============================================================================
add_subdirectory(ThirdParty)

include(Render/Shader/AtomShaders.cmake)

# ============================================================================
# MinGW runtime linking
# ============================================================================
# Both settings must be in place before the first Atom target is created and
# before add_subdirectory(Test): CMake captures these variables per directory
# scope, so setting them further down this file silently skips every target
# already defined. That is exactly how the test executables ended up dynamically
# linked against libstdc++-6.dll / libgcc_s_seh-1.dll with no copy of either DLL
# produced anywhere, while Atom.exe got both flags and worked.
if(MINGW)
    # Statically link libgcc and libstdc++ so executables do not require
    # libgcc_s_seh-1.dll / libstdc++-6.dll. Plain "-static" is avoided: this
    # toolchain's static winpthread archive is built for UCRT and references
    # __intrinsic_setjmpex, which the MSVCRT runtime sequence does not provide
    # (undefined reference at link time).
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

    # libwinpthread stays a DLL, and Windows resolves it next to the loading
    # executable rather than in a parent directory -- so tests built into Test/
    # could never see the copy made for the build root. Collecting every Atom
    # executable in one directory lets a single copy serve all of them.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()


# ============================================================================
# Engine Libraries
# ============================================================================
# 0. Input value types (header-only, no platform/backend dependency).
    add_library(Atom_Input INTERFACE)
    target_sources(Atom_Input INTERFACE
        Input/Key.hpp
        Input/ActionMap.hpp
    )
    target_include_directories(Atom_Input INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

# 0a. Shared color value and operations (header-only, no renderer dependency).
    # Lives under Algorithm/ as a pure value/math type, not a root subsystem.
    add_library(Atom_Color INTERFACE)
    target_sources(Atom_Color INTERFACE
        Algorithm/Color/Color.hpp
        Algorithm/Color/ColorMath.hpp
    )
    target_include_directories(Atom_Color INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# 0a-bis. Math — pure value/math types with no engine dependency at all (A0.2).
#     Split out of Atom_Algorithm so the backend contracts can use Vec2/Mat3/Rect
#     without depending on the higher "algorithm" module (transforms, cameras).
#     Atom_Algorithm keeps the behaviour-bearing types and links this one.
#     Files stay under Algorithm/ because that is where they have always lived;
#     the ownership move is a target-graph change, not a directory move.
    add_library(Atom_Math STATIC
        Algorithm/Vector/Vec2.cpp
        Algorithm/Vector/Vec2.hpp
        Algorithm/Vector/Vec3.cpp
        Algorithm/Vector/Vec3.hpp
        Algorithm/Vector/Vec4.cpp
        Algorithm/Vector/Vec4.hpp
        Algorithm/Matrix/Mat3.cpp
        Algorithm/Matrix/Mat3.hpp
        Algorithm/Matrix/Mat4.cpp
        Algorithm/Matrix/Mat4.hpp
        Algorithm/Geometry/Primitives.hpp
        Algorithm/Geometry/Rect.hpp
        Algorithm/Math/Scalar.hpp
        Algorithm/Interpolation/Easing.hpp
    )
    target_include_directories(Atom_Math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # Color is a header-only value type; Vec2/Mat3 do not use it, but keeping the
    # edge here means every math consumer gets the full value vocabulary.
    target_link_libraries(Atom_Math PUBLIC Atom_Color)

# 0b. Interfaces (header-only, no SDL3 dependency)
    add_library(Atom_Backend_Contracts INTERFACE)
    target_sources(Atom_Backend_Contracts INTERFACE
        Backend/Contracts/Audio/AudioTypes.hpp
        Backend/Contracts/Audio/AudioExtensions.hpp
        Backend/Contracts/Audio/AudioBackendId.hpp
        Backend/Contracts/Audio/AudioSourceRegistry.hpp
        Backend/Contracts/Audio/IAudioBackend.hpp
        Backend/Contracts/Audio/IAudioDecoder.hpp
        Backend/Contracts/Audio/IAudioSource.hpp
        Backend/Contracts/Audio/NullAudioBackend.hpp
        Backend/Contracts/Debug/IDebugImGuiBackend.hpp
        Backend/Contracts/Numeric/NumericContract.hpp
        Backend/Contracts/Render/IRender2DContext.hpp
        Backend/Contracts/Render/IRenderBackend.hpp
        Backend/Contracts/Render/IRenderDevice.hpp
        Backend/Contracts/Render/IRenderTarget.hpp
        Backend/Contracts/Render/IRenderTypes.hpp
        Backend/Contracts/Render/ITexture.hpp
        Backend/Contracts/Render/PresentMode.hpp
        Backend/Contracts/Render/RenderBackendConfig.hpp
        Backend/Contracts/Render/RenderBackendId.hpp
        Backend/Contracts/Time/ITimeSource.hpp
        Backend/Contracts/Video/IVideoDecoder.hpp
        Backend/Contracts/Window/IWindow.hpp
        Backend/Contracts/Window/WindowMode.hpp
    )
    target_include_directories(Atom_Backend_Contracts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    # A0.2: the contracts' public headers include Algorithm/Vector/Vec2.hpp,
    # Algorithm/Color/Color.hpp and Input/Key.hpp, and Audio's decoder contract
    # includes Filesystem/FileSystem.hpp for IFile. Every one of those is now a
    # declared dependency instead of an include path that merely happens to
    # resolve in a single-tree checkout. Atom_FS is attached further down, next
    # to the Atom_FS definition, so no target is referenced before it exists.
    target_link_libraries(Atom_Backend_Contracts INTERFACE Atom_Math Atom_Color Atom_Input)

# 0b. Error — the engine's unified failure value (CORE-004).
#     Leaf module: no engine dependency at all, not even Log. Every other
#     module may link it, and it must never link anything back. Defined before
#     Log because Log consumes Error codes for its own diagnostics.
    add_library(Atom_Error STATIC
        Error/Error.hpp
        Error/Error.cpp
    )
    target_include_directories(Atom_Error PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # A0.3: Atom_Error is platform-free by contract. It must stay linkable
    # without any backend compiled in, so SDL error capture lives in
    # Backend/SDL3/Core/SdlError.hpp instead of being exported from here. This
    # is what lets Filesystem/Asset/ECS/Scene tests link with no SDL backend.

# 0b-bis. Input system — per-frame keyboard/mouse state folded from the window
#     event stream. Consumes window::IEvent, so it sits above the contracts.
    add_library(Atom_InputSystem STATIC
        Input/InputSystem.hpp
        Input/InputSystem.cpp
    )
    target_include_directories(Atom_InputSystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # Atom_Log is PUBLIC because a consumer that logs its own InputSystem reads
    # (Log/AtomLogChannels.hpp) resolves the channel symbols in its own objects.
    target_link_libraries(Atom_InputSystem PUBLIC Atom_Backend_Contracts Atom_Log)

# 0c. Time (master + domain clocks, fixed-step policy)
    add_library(Atom_Time STATIC
            Time/TimeTypes.hpp
            Time/SteadyTimeSource.hpp
            Time/MasterClock.hpp
            Time/MasterClock.cpp
            Time/DomainClock.hpp
            Time/DomainClock.cpp
            Time/TimeSystem.hpp
            Time/TimeSystem.cpp
    )
    target_include_directories(Atom_Time PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Time PUBLIC Atom_Backend_Contracts)

# 0c-bis. ThreadRegistry — process-wide registry of long-lived threads for
#     diagnostics (which thread is alive, which one burns the most CPU). A leaf
#     module with no dependency on Log or Concurrency, so Log's own writer
#     thread can register without a cycle.
    add_library(Atom_Threading STATIC
        Concurrency/ThreadRegistry.hpp
        Concurrency/ThreadRegistry.cpp
    )
    target_include_directories(Atom_Threading PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 0d. Concurrency — work-stealing job scheduler over a fixed worker pool.
#     Standalone foundation with no dependency on Log/Time/backend; every
#     subsystem (audio decode, asset loading, physics, AI) can schedule work
#     through it. Header-only deque + job types, with a compiled scheduler.
    add_library(Atom_Concurrency STATIC
        Concurrency/Job.hpp
        Concurrency/WorkStealingDeque.hpp
        Concurrency/JobSystem.hpp
        Concurrency/JobSystem.cpp
        Concurrency/Future.hpp
    )
    target_include_directories(Atom_Concurrency PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Concurrency PUBLIC Atom_Threading)

# 1. Log
    add_library(Atom_Log STATIC
            Log/LogSystem.cpp
            Log/LogSystem.hpp
            Log/LogChannelMacros.hpp
            Log/AtomLogChannels.hpp
    )
    target_include_directories(Atom_Log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # A0.2: Log's headers include no backend contract. The previous
    # "PUBLIC Atom_Backend_Contracts" here was a stale edge that pulled the whole
    # contract (and now Filesystem) into every Log consumer, ECS included.
    target_link_libraries(Atom_Log PUBLIC Atom_Threading)

    # The job scheduler logs lifecycle + swallowed-exception diagnostics, so it
    # depends on the log runtime. PUBLIC so any JobSystem consumer (e.g. the
    # standalone tour example) links the log symbols transitively.
    target_link_libraries(Atom_Concurrency PUBLIC Atom_Log)

# 2-bis. ECS — engine-owned entity/component storage.
#     Atom_3rd_EnTT is PRIVATE on purpose: the backend must not reach any public
#     header, and a PUBLIC link would put its include directory on the path of
#     every Atom_ECS consumer, turning the boundary into a comment.
    add_library(Atom_ECS STATIC
            ECS/Entity.hpp
            ECS/TypeName.hpp
            ECS/Components.hpp
            ECS/EntityWorld.hpp
            ECS/EntityWorld.cpp
    )
    target_include_directories(Atom_ECS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # A0.1: Components.hpp is a public header that includes Algorithm/Vector/Vec2.hpp
    # and Algorithm/Color/Color.hpp, so both value targets must be PUBLIC -
    # previously only Atom_Algorithm was linked PRIVATE, which meant the dependency
    # was real but undeclared. Atom_Math is the leaf that now owns Vec2, so ECS no
    # longer reaches the transform/camera module.
    # Admission rules for ATOM_ECS_COMPONENTS: Docs/Plans/ECS-Component-Vocabulary-CN.md
    target_link_libraries(Atom_ECS PUBLIC Atom_Error Atom_Log Atom_Math Atom_Color)
    target_link_libraries(Atom_ECS PRIVATE Atom_3rd_EnTT)

# 2. Algorithm — behaviour-bearing math (transforms, cameras).
#     A0.2: pure values moved to Atom_Math. This target used to link
#     Atom_Backend_Contracts although not one of its headers includes anything
#     from Backend/, which is what closed the Algorithm <-> Contracts cycle.
    add_library(Atom_Algorithm STATIC
            Algorithm/Transform/Transform2D.cpp
            Algorithm/Transform/Transform2D.hpp
            Algorithm/Transform/Transform3D.cpp
            Algorithm/Transform/Transform3D.hpp
            Algorithm/Camera/Camera2D.cpp
            Algorithm/Camera/Camera2D.hpp
            Algorithm/Camera/Camera3D.cpp
            Algorithm/Camera/Camera3D.hpp
    )
    target_include_directories(Atom_Algorithm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Algorithm PUBLIC Atom_Math)

# 2b. Layout
    add_library(Atom_Layout STATIC
        Layout/LayoutConfig.cpp
        Layout/LayoutConfig.hpp
        Layout/LayoutNode.cpp
        Layout/LayoutNode.hpp
        Layout/LayoutTree.cpp
        Layout/LayoutTree.hpp
        Layout/LayoutTypes.hpp
    )
    target_include_directories(Atom_Layout PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Layout PRIVATE Atom_3rd_Yoga Atom_Log)

# 2c. Physics — collision shapes, mass properties, and the replaceable-backend
#     contract. M1 ships pure shape/mass math + the IPhysics* contract; the
#     Internal solver and world land in later phases (see Docs/Physics-Plan-CN.md).
    add_library(Atom_Physics STATIC
        Physics/Shapes/Material.hpp
        Physics/Shapes/MassProperties.hpp
        Physics/Shapes/Shape.hpp
        Physics/Shapes/Shape.cpp
        Physics/Dynamics/RigidBody.hpp
        Physics/Dynamics/RigidBody.cpp
        Physics/Collision/BroadPhase.hpp
        Physics/Collision/BroadPhase.cpp
        Physics/Collision/Contact.hpp
        Physics/Collision/NarrowPhase.hpp
        Physics/Collision/NarrowPhase.cpp
        Physics/Solver/ContactSolver.hpp
        Physics/Solver/ContactSolver.cpp
        Physics/Solver/RevoluteJoint.hpp
        Physics/Solver/RevoluteJoint.cpp
        Physics/PhysicsWorld/FixedStepper.hpp
        Physics/Contracts/PhysicsTypes.hpp
        Physics/Contracts/IPhysicsBody.hpp
        Physics/Contracts/IPhysicsWorld.hpp
        Physics/Contracts/IPhysicsJoint.hpp
        Physics/Contracts/IPhysicsBackend.hpp
        Physics/Contracts/PhysicsBackendId.hpp
        Physics/Backend/Internal/InternalPhysicsBody.hpp
        Physics/Backend/Internal/InternalPhysicsBody.cpp
        Physics/Backend/Internal/InternalPhysicsWorld.hpp
        Physics/Backend/Internal/InternalPhysicsWorld.cpp
        Physics/Backend/Internal/InternalRevoluteJoint.hpp
        Physics/Backend/Internal/InternalRevoluteJoint.cpp
        Physics/Backend/Internal/InternalPhysicsBackend.hpp
        Physics/Backend/Internal/InternalPhysicsBackend.cpp
    )
    target_include_directories(Atom_Physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # Physics can fork pure narrow-phase batches through the engine scheduler.
    # The scheduler remains an implementation detail: IPhysics* headers do not
    # expose JobSystem types and callers retain a serial fallback when it is off.
    target_link_libraries(Atom_Physics PUBLIC Atom_Algorithm Atom_Error Atom_Log)
    target_link_libraries(Atom_Physics PRIVATE Atom_Concurrency)

# 3. SDL3 Core
    add_library(Atom_Backend_SDL3_Core STATIC
        Backend/SDL3/Core/SDLRuntime.cpp
        Backend/SDL3/Core/SDLRuntime.hpp
        Backend/SDL3/Core/SdlError.cpp
        Backend/SDL3/Core/SdlError.hpp
        Backend/SDL3/Time/SDL3TimeSource.cpp
        Backend/SDL3/Time/SDL3TimeSource.hpp
    )
    target_include_directories(Atom_Backend_SDL3_Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # SDL3TimeSource implements backend::ITimeSource, so the contract library is
    # a real dependency here, not just an include path that happens to resolve.
    target_link_libraries(Atom_Backend_SDL3_Core PUBLIC Atom_3rd_SDL3 Atom_Backend_Contracts)

# 3a. SDL3 window platform layer
    add_library(Atom_Backend_SDL3_Window STATIC
        Backend/SDL3/Window/SDL3Window.cpp
        Backend/SDL3/Window/SDL3Window.hpp
    )
    target_include_directories(Atom_Backend_SDL3_Window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_SDL3_Window PUBLIC Atom_Backend_Contracts Atom_Backend_SDL3_Core Atom_Algorithm Atom_Log)

# 3b. SDL_GPU render device and backend composition.
    add_library(Atom_Backend_SDL_GPU STATIC
        Backend/SDLGPU/Device/SDLGPUDevice.cpp
        Backend/SDLGPU/Device/SDLGPUDevice.hpp
        Backend/SDLGPU/Device/SDLGPUBackend.cpp
        Backend/SDLGPU/Device/SDLGPUBackend.hpp
        Backend/SDLGPU/Encoding/SDLGPU2D.cpp
        Backend/SDLGPU/Pipelines/SDLGPU2DPipelineFactory.cpp
        Backend/SDLGPU/Pipelines/SDLGPU2DPipelineFactory.hpp
        Backend/SDLGPU/Pipelines/SDLGPUShaderLoader.cpp
        Backend/SDLGPU/Pipelines/SDLGPUShaderLoader.hpp
    )
    target_include_directories(Atom_Backend_SDL_GPU PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_SDL_GPU PUBLIC Atom_Backend_Contracts Atom_Backend_SDL3_Window Atom_Log Atom_3rd_SDL3)
    target_compile_definitions(Atom_Backend_SDL_GPU PRIVATE
        ATOM_SHADER_HAS_DXIL=$<BOOL:${ATOM_SHADER_BUILD_DXIL}>
        ATOM_SHADER_HAS_MSL=$<BOOL:${ATOM_SHADER_BUILD_MSL}>)

# 3c. SDL_GPU Debug (ImGui) Backend — ImGui glue for the SDL_GPU render backend.
    add_library(Atom_Backend_SDLGPU_Debug STATIC
        Backend/SDLGPU/Debug/SDLGPUImGuiBackend.cpp
        Backend/SDLGPU/Debug/SDLGPUImGuiBackend.hpp
    )
    target_include_directories(Atom_Backend_SDLGPU_Debug PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_SDLGPU_Debug PUBLIC
        Atom_Backend_Contracts Atom_Backend_SDL_GPU Atom_3rd_ImGui_SDL3)

# 3d. Render Backend Runtime — the sanctioned layer that knows the concrete
#     backends and registers them (window + render-device factory);
#     upper layers only consume the registries and the I* interfaces.
    add_library(Atom_Backend_Render_Composition STATIC
        Backend/Composition/RenderBackendRuntime.cpp
        Backend/Composition/RenderBackendRuntime.hpp
    )
    target_include_directories(Atom_Backend_Render_Composition PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_Render_Composition PUBLIC Atom_Backend_Extension Atom_Backend_SDL_GPU)
    target_link_libraries(Atom_Backend_Render_Composition PRIVATE Atom_Backend_SDLGPU_Debug)

    # Compatibility only; new code should name the composition role directly.
    add_library(Atom_Backend_Render_Runtime INTERFACE)
    target_link_libraries(Atom_Backend_Render_Runtime INTERFACE Atom_Backend_Render_Composition)

# 3e. Text — backend-neutral font provider and glyph rasterization service.
    add_library(Atom_Text STATIC
        Render/Text/Font.cpp
        Render/Text/Font.hpp
        Render/Text/FontFileLoader.cpp
        Render/Text/FontFileLoader.hpp
        Render/Text/FreeTypeFontProvider.cpp
        Render/Text/FreeTypeFontProvider.hpp
        Render/Text/HarfBuzzTextShaper.cpp
        Render/Text/HarfBuzzTextShaper.hpp
        Render/Text/TextLayoutBuilder.cpp
        Render/Text/TextLayoutBuilder.hpp
    )
    target_include_directories(Atom_Text PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Text PRIVATE Atom_3rd_FreeType Atom_3rd_HarfBuzz Atom_Concurrency)

# 3f. Pipeline — backend-neutral pipeline descriptors shared by 2D and 3D.
    add_library(Atom_Render_Pipeline INTERFACE)
    target_sources(Atom_Render_Pipeline INTERFACE
        Render/Pipeline/PipelineTypes.hpp
    )
    target_include_directories(Atom_Render_Pipeline INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# 3g. Render Core — backend-neutral pass scheduling and render orchestration.
    add_library(Atom_Render_Core STATIC
        Render/Core/RenderGraph.cpp
        Render/Core/RenderGraph.hpp
    )
    target_include_directories(Atom_Render_Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 3h. Renderer2D — high-level batched 2D renderer on the IRender2DContext slice.
    add_library(Atom_Renderer2D STATIC
        Render/Renderer2D/Renderer2D.cpp
        Render/Renderer2D/Renderer2D.hpp
    )
    target_include_directories(Atom_Renderer2D PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Renderer2D PUBLIC Atom_Backend_Contracts Atom_Render_Pipeline Atom_Render_Core Atom_Text)
    target_link_libraries(Atom_Renderer2D PRIVATE Atom_Log)

# 3h-bis. Game UI — retained widgets over Layout, normalized input, and a
# screen-space Renderer2D canvas. Runtime-owned pages may consume this module;
# UI itself has no dependency on Runtime, Window implementations, ECS, or ImGui.
    add_library(Atom_UI STATIC
        UI/UiTree.hpp
        UI/UiTree.cpp
        UI/UiCanvas.hpp
        UI/UiCanvas.cpp
    )
    target_include_directories(Atom_UI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_UI PUBLIC Atom_Layout Atom_InputSystem Atom_Renderer2D)

# 3i. Render resources — bridges decoded media assets into renderer-owned GPU resources.
    add_library(Atom_Render_Resources STATIC
        Render/Resources/ImageTexture.hpp
        Render/Resources/ImageTexture.cpp
        Render/Resources/TextureCache.hpp
        Render/Resources/TextureCache.cpp
    )
    target_include_directories(Atom_Render_Resources PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Render_Resources PUBLIC Atom_Renderer2D Atom_Image)
    # Async single-image loads (TextureCache::LoadAsync) decode on the job
    # system. JobSystem is an implementation detail of TextureCache.cpp, so the
    # link stays PRIVATE; consumers only see JobHandle by value in the header.
    target_link_libraries(Atom_Render_Resources PRIVATE Atom_Concurrency)

# 3j. Scene2D — ECS sprites and a 2D camera turned into one Renderer2D pass.
#     Depends on ECS + Renderer2D + render resources, and never on Runtime: a
#     scene renderer must be usable by a tool or a test with no frame loop.
    add_library(Atom_Scene2D STATIC
        Render/Scene2D/Scene2DTypes.hpp
        Render/Scene2D/Scene2DRenderer.hpp
        Render/Scene2D/Scene2DRenderer.cpp
        Render/Scene2D/TextureCacheResolver.hpp
        Render/Scene2D/TextureCacheResolver.cpp
    )
    target_include_directories(Atom_Scene2D PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Scene2D PUBLIC Atom_ECS Atom_Renderer2D Atom_Render_Resources)

# 3k. Json — a strict JSONC reader, owned by Utilities because every document format
#     needs one (scenes today; sprite metadata, prefabs and save data next). A leaf:
#     standard library + Error only, so any layer may use it.
    add_library(Atom_Json STATIC
        Utilities/Json/JsonReader.hpp
        Utilities/Json/JsonReader.cpp
    )
    target_include_directories(Atom_Json PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Json PUBLIC Atom_Error)

# 3k-bis. Sprite — the art pipeline's metadata layer: one texture, named frames,
#     one filter. Depends on Json + FS + Math/Color, NOT on ECS: an atlas is asset
#     data, and a component must not need to know what an atlas is.
    add_library(Atom_Sprite STATIC
        Sprite/SpriteAtlas.hpp
        Sprite/SpriteAtlas.cpp
        Sprite/SpriteAtlasLoader.hpp
        Sprite/SpriteAtlasLoader.cpp
    )
    target_include_directories(Atom_Sprite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Sprite PUBLIC Atom_Json Atom_FS Atom_Log Atom_Math Atom_Color)

# 3l. Scene — the level document format (T2.1) and its loader (T2.2).
#     Depends on ECS + FS + Json and NOT on Runtime: a loader is usable by a tool or
#     a test with no frame loop, and the switch is a single
#     GameWorld::ReplaceEntities call made by the game code.
    add_library(Atom_Scene STATIC
        Scene/SceneTypes.hpp
        Scene/SceneSchema.hpp
        Scene/SceneSchema.cpp
        Scene/SceneLoader.hpp
        Scene/SceneLoader.cpp
    )
    target_include_directories(Atom_Scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # The schema and the loader are public; the JSON DOM stays behind SceneSchema, and
    # nothing in ECS, Render or Runtime may name it.
    target_link_libraries(Atom_Scene PUBLIC Atom_ECS Atom_FS Atom_Json Atom_Log Atom_Math Atom_Color
        Atom_Sprite)

# 4. Backend extension registration and format codecs
    add_library(Atom_Backend_Extension STATIC
        Backend/Extension/AudioDecoderRegistry.hpp
        Backend/Extension/AudioDecoderRegistry.cpp
        Backend/Extension/BackendRegistry.hpp
        Backend/Extension/BackendRegistry.cpp
        Backend/Extension/RenderBackendRegistry.hpp
        Backend/Extension/RenderBackendRegistry.cpp
        Backend/Extension/DebugImGuiBackendRegistry.hpp
        Backend/Extension/DebugImGuiBackendRegistry.cpp
    )
    target_include_directories(Atom_Backend_Extension PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_Extension PUBLIC Atom_Backend_Contracts)

    # Codecs parse media formats into PCM; they are not playback backends.
    add_library(Atom_Audio_Codecs STATIC
        Media/Audio/Codecs/WavProf/RiffWaveReader.hpp
        Media/Audio/Codecs/WavProf/RiffWaveReader.cpp
        Media/Audio/Codecs/WavProf/WavProfDecoder.hpp
        Media/Audio/Codecs/WavProf/WavProfDecoder.cpp
        Media/Audio/Codecs/SDL3Wav/SDL3WavDecoder.hpp
        Media/Audio/Codecs/SDL3Wav/SDL3WavDecoder.cpp
        Media/Audio/Codecs/minimp3/Minimp3Decoder.hpp
        Media/Audio/Codecs/minimp3/Minimp3Decoder.cpp
    )
    target_include_directories(Atom_Audio_Codecs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Audio_Codecs PUBLIC Atom_Backend_Contracts Atom_Backend_Extension Atom_3rd_Minimp3 Atom_UTF8 Atom_3rd_SDL3)

    # Compatibility target for out-of-tree consumers. New CMake code must use
    # Atom_Audio_Codecs so the target name matches its Media/Audio location.
    add_library(Atom_Backend_Audio INTERFACE)
    target_link_libraries(Atom_Backend_Audio INTERFACE Atom_Audio_Codecs)

# 5. SDL3 Audio Backend
    add_library(Atom_Backend_SDL3_Audio STATIC
        Backend/SDL3/Audio/SDL3AudioBackend.cpp
        Backend/SDL3/Audio/SDL3AudioBackend.hpp
        Backend/SDL3/Audio/SDL3SFXSource.cpp
        Backend/SDL3/Audio/SDL3SFXSource.hpp
        Backend/SDL3/Audio/SDL3MusicSource.cpp
        Backend/SDL3/Audio/SDL3MusicSource.hpp
        Backend/SDL3/Audio/SDL3StreamingMusicSource.cpp
        Backend/SDL3/Audio/SDL3StreamingMusicSource.hpp
    )
    target_include_directories(Atom_Backend_SDL3_Audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_SDL3_Audio PUBLIC Atom_Backend_Contracts Atom_Backend_Extension Atom_Backend_SDL3_Core Atom_Log)

    add_library(Atom_Backend_SDL3_Mixer_Audio STATIC
        Backend/SDL3/Audio/SDL3MixerAudioBackend.cpp
        Backend/SDL3/Audio/SDL3MixerAudioBackend.hpp
        Backend/SDL3/Audio/SDL3MixerContext.cpp
        Backend/SDL3/Audio/SDL3MixerContext.hpp
        Backend/SDL3/Audio/SDL3MixerSource.cpp
        Backend/SDL3/Audio/SDL3MixerSource.hpp
        Backend/SDL3/Audio/SDL3MixerStreamingSource.cpp
        Backend/SDL3/Audio/SDL3MixerStreamingSource.hpp
    )
    target_include_directories(Atom_Backend_SDL3_Mixer_Audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_SDL3_Mixer_Audio PUBLIC Atom_Backend_Contracts Atom_Backend_SDL3_Core Atom_Log Atom_3rd_SDL3_mixer)

    add_library(Atom_Backend_Composition STATIC
        Backend/Composition/BackendRuntime.hpp
        Backend/Composition/BackendRuntime.cpp
        Backend/Composition/IAudioBackendChangeListener.hpp
    )
    target_include_directories(Atom_Backend_Composition PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
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

# 6. Media
    add_library(Atom_Media STATIC
        Media/Audio/AudioBackend.cpp
        Media/Audio/AudioBackend.hpp
        Media/Audio/Metadata/AudioMetadataReader.cpp
        Media/Audio/Metadata/AudioMetadataReader.hpp
        Media/Audio/Effects/IAudioEffect.hpp
        Media/Audio/Effects/DopplerEffect.cpp
        Media/Audio/Effects/DopplerEffect.hpp
        Media/Audio/Mixing/AudioMixer.cpp
        Media/Audio/Mixing/AudioMixer.hpp
        Media/Audio/Resources/AudioClipLoader.cpp
        Media/Audio/Resources/AudioClipLoader.hpp
        Media/Audio/Resources/AudioClipResourceLoader.cpp
        Media/Audio/Resources/AudioClipResourceLoader.hpp
        Media/Audio/Resources/AudioClipCache.cpp
        Media/Audio/Resources/AudioClipCache.hpp
        Media/Audio/Playback/MusicPlayer.cpp
        Media/Audio/Playback/MusicPlayer.hpp
        Media/Audio/Playback/SFXPlayer.cpp
        Media/Audio/Playback/SFXPlayer.hpp
        Media/Audio/Playback/VoicePool.cpp
        Media/Audio/Playback/VoicePool.hpp
        Media/Audio/Transitions/MusicCrossfade.cpp
        Media/Audio/Transitions/MusicCrossfade.hpp
    )
    target_include_directories(Atom_Media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Media PUBLIC Atom_Backend_Contracts Atom_Backend_Extension Atom_Backend_Composition Atom_Concurrency Atom_Log Atom_UTF8 Atom_3rd_TagLib)

# 7. UTF-8 encoding utilities
add_library(Atom_UTF8 STATIC
    Utilities/Utf8/Utf8.hpp
    Utilities/Utf8/Utf8.cpp
)
target_include_directories(Atom_UTF8 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# A0.2: Utf8 includes no backend contract. The stale edge to
# Atom_Backend_Contracts used to make Filesystem -> Utf8 -> Contracts a cycle
# once the contracts declared their real Filesystem dependency below.
target_link_libraries(Atom_UTF8 PUBLIC Atom_3rd_UtfCpp)

# 7b. Platform-independent virtual filesystem. Native implementation details
# remain private; consumers use AssetPath and IFileSystem only.
add_library(Atom_FS STATIC
    Filesystem/AssetPath.hpp
    Filesystem/AssetPath.cpp
    Filesystem/FileSystem.hpp
    Filesystem/FileSystem.cpp
    Filesystem/MemoryFileSystem.hpp
    Filesystem/MemoryFileSystem.cpp
    Filesystem/OverlayFileSystem.hpp
    Filesystem/OverlayFileSystem.cpp
    Filesystem/Vfs.hpp
    Filesystem/Vfs.cpp
    Filesystem/PackageFormat.hpp
    Filesystem/PackageFileSystem.hpp
    Filesystem/PackageFileSystem.cpp
)
target_include_directories(Atom_FS PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Atom_Error is PUBLIC: AssetPath::Parse returns Result<AssetPath>, so every
# consumer of the FS public headers needs the error type to be complete.
target_link_libraries(Atom_FS PUBLIC Atom_Error)
target_link_libraries(Atom_FS PRIVATE Atom_UTF8)

# A0.2: the audio decoder contract declares IFile, so the contracts own a real
# (and acyclic) dependency on the filesystem: Contracts -> FS -> Error/Utf8, with
# no edge coming back. Declared here because Atom_FS is defined in this block.
target_link_libraries(Atom_Backend_Contracts INTERFACE Atom_FS)
# Render resources need AssetPath/IFileSystem for VFS-backed texture loads.
target_link_libraries(Atom_Render_Resources PUBLIC Atom_FS)
# Audio codecs and the clip loader consume IFile for VFS-backed audio (ARCH-107 C3).
target_link_libraries(Atom_Audio_Codecs PUBLIC Atom_FS)
target_link_libraries(Atom_Media PUBLIC Atom_FS)

# 7c. Resource identity, handles, loader registry, and identity-keyed cache.
add_library(Atom_Assets STATIC
    Asset/AssetKind.hpp
    Asset/AssetKind.cpp
    Asset/ResourceId.hpp
    Asset/ResourceId.cpp
    Asset/ResourceHandle.hpp
    Asset/IResourceLoader.hpp
    Asset/IResourceLoader.cpp
    Asset/ResourceManager.hpp
    Asset/ResourceManager.cpp
    Asset/ScriptSourceLoader.hpp
    Asset/ScriptSourceLoader.cpp
)
target_include_directories(Atom_Assets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Atom_Log is needed because a failed acquire reports itself exactly once, at
# the site that knows the operation and the resource (CORE-004).
target_link_libraries(Atom_Assets PUBLIC Atom_FS Atom_Log)
# Batch acquire spreads the per-file load (I/O + decode) across the job system,
# so ResourceManager depends on the scheduler (implementation detail, not API).
target_link_libraries(Atom_Assets PRIVATE Atom_Concurrency)
# FontFileLoader is a TypedResourceLoader, so font bytes enter the same
# URI/identity + async-acquire machinery as textures and audio (ARCH-107 P4).
# Declared here (after Atom_Assets exists) because Atom_Text is defined earlier.
target_link_libraries(Atom_Text PUBLIC Atom_Assets)
# AudioClipResourceLoader (kind=Audio) lives in Atom_Media and extends
# atom::asset::TypedResourceLoader, mirroring DecodedImageLoader in Atom_Image.
target_link_libraries(Atom_Media PUBLIC Atom_Assets)

# 7a. Image decoding (CPU-side; GPU upload is a separate step via Renderer2D).
add_library(Atom_Image STATIC
    Media/Image/ImageDecoder.hpp
    Media/Image/ImageDecoder.cpp
        Media/Image/Analysis/ColorStatistics.hpp
        Media/Image/Analysis/ColorStatistics.cpp
    Media/Image/DecodedImageLoader.hpp
    Media/Image/DecodedImageLoader.cpp
    Media/Image/ImageBatchDecoder.hpp
    Media/Image/ImageBatchDecoder.cpp
)
target_include_directories(Atom_Image PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Atom_Image PUBLIC Atom_Color Atom_Assets)
target_link_libraries(Atom_Image PRIVATE Atom_3rd_Stb Atom_Log Atom_Concurrency)

# 8. Packager
    add_library(Atom_Packager STATIC
            Utilities/Packager/PackageBuilder.cpp
            Utilities/Packager/PackageBuilder.hpp
    )
    target_include_directories(Atom_Packager PUBLIC Utilities/Packager)
    target_link_libraries(Atom_Packager PUBLIC Atom_UTF8 Atom_Log)

# 9. Window/render host. It owns native window, backend and frame submission;
# Runtime owns the frame loop, time domains and screen dispatch.
    add_library(Atom_Window STATIC
            Window/RenderWindow.hpp
            Window/RenderWindow.cpp
    )
    target_include_directories(Atom_Window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Window PUBLIC
        Atom_Backend_Contracts
        Atom_Error
        Atom_InputSystem
        Atom_Log
        Atom_Algorithm)
    target_link_libraries(Atom_Window PRIVATE
        Atom_Backend_Extension
        Atom_Backend_Render_Composition)

# 9b. Display policy. Owns the two player-facing presentation choices (window
# placement + present mode) and the frame-boundary application of a change.
# It sits above the contracts (it only needs IWindow/IRenderDevice) and below
# Runtime (which ticks it), so neither the window facade nor a debug panel has
# to own the state.
    add_library(Atom_Display STATIC
            Display/DisplaySettings.hpp
            Display/DisplaySystem.hpp
            Display/DisplaySystem.cpp
    )
    target_include_directories(Atom_Display PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Display PUBLIC Atom_Backend_Contracts Atom_Log)

# 10. Runtime. This is the sole game frame-loop owner; Window intentionally
# remains usable by tools that only need a platform/render host.
    add_library(Atom_Runtime STATIC
            Runtime/Application.hpp
            Runtime/Application.cpp
            Runtime/EntityCommandBuffer.hpp
            Runtime/EntityCommandBuffer.cpp
            Runtime/GameEventQueue.hpp
            Runtime/GameEventQueue.cpp
            Runtime/GameScreen.hpp
            Runtime/GameScreen.cpp
            Runtime/GameWorld.hpp
            Runtime/GameWorld.cpp
            Runtime/Screen.hpp
            Runtime/ScreenManager.hpp
            Runtime/ScreenManager.cpp
            Runtime/System.hpp
            Runtime/SystemScheduler.hpp
            Runtime/SystemScheduler.cpp
    )
    target_include_directories(Atom_Runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Runtime PUBLIC Atom_Window Atom_Display Atom_Time Atom_Log Atom_ECS)

# 11. Debugger
    # Debug overlay build switch. AUTO (default) follows the build type:
    # enabled in Debug, stripped in Release/MinSizeRel. Force either way with
    # -DATOM_ENABLE_DEBUGGER=ON (e.g. to ship a Release build that still shows
    # the debug overlay, or a future profiler built on the same panels) or
    # =OFF. The resolved value is propagated as an explicit 0/1 macro so no
    # translation unit relies on the header-side NDEBUG heuristic.
    set(ATOM_ENABLE_DEBUGGER "AUTO" CACHE STRING "Debug overlay: AUTO (follow build type), ON, or OFF")
    set_property(CACHE ATOM_ENABLE_DEBUGGER PROPERTY STRINGS AUTO ON OFF)

    set(_atom_debugger_enabled OFF)
    if(ATOM_ENABLE_DEBUGGER STREQUAL "ON")
        set(_atom_debugger_enabled ON)
    elseif(ATOM_ENABLE_DEBUGGER STREQUAL "AUTO")
        # Single-config Ninja builds set CMAKE_BUILD_TYPE; the header fallback in
        # DebuggerConfig.hpp still covers non-CMake or multi-config consumers.
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
            set(_atom_debugger_enabled ON)
        endif()
    endif()

    add_library(Atom_Debugger STATIC
            Debugger/DebugPanel.hpp
            Debugger/DebugPanel.cpp
            Debugger/Panels/LogDebugger.hpp
            Debugger/Panels/LogDebugger.cpp
            Debugger/FontLoader.hpp
            Debugger/FontLoader.cpp
            Debugger/PanelLayout.hpp
            Debugger/PanelLayout.cpp
            Debugger/Overlay.hpp
            Debugger/OverlayManager.hpp
            Debugger/OverlayManager.cpp
            Debugger/Panels/JobDebugger.hpp
            Debugger/Panels/JobDebugger.cpp
            Debugger/Panels/ThreadDebugger.hpp
            Debugger/Panels/ThreadDebugger.cpp
            Debugger/Panels/FpsDebugger.hpp
            Debugger/Panels/FpsDebugger.cpp
            Debugger/Panels/RenderDebugger.hpp
            Debugger/Panels/RenderDebugger.cpp
            Debugger/Panels/DisplayDebugger.hpp
            Debugger/Panels/DisplayDebugger.cpp
    )
    target_include_directories(Atom_Debugger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(Atom_Debugger PUBLIC ATOM_ENABLE_DEBUGGER=$<BOOL:${_atom_debugger_enabled}>)
    target_compile_definitions(Atom_Window PUBLIC ATOM_ENABLE_DEBUGGER=$<BOOL:${_atom_debugger_enabled}>)
    target_link_libraries(Atom_Debugger PUBLIC Atom_Runtime Atom_3rd_ImGui_SDL3 Atom_Concurrency)
    target_link_libraries(Atom_Debugger PRIVATE Atom_Backend_SDLGPU_Debug)
    if(WIN32)
        target_link_libraries(Atom_Debugger PUBLIC opengl32 gdi32)
    endif()

# 12. Convenience package
    # Atom_All is intentionally a convenience package for applications and
    # legacy samples. Engine modules and new samples must link the smallest
    # explicit set of domain targets they consume.
    add_library(Atom_All INTERFACE)
    target_include_directories(Atom_All INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_All INTERFACE
            Atom_Algorithm
            # Time is consumed directly by upper layers through
            # TimeSystem::GetInstance(), not only transitively via Atom_Window.
            Atom_Time
            Atom_Concurrency
            Atom_Layout
            Atom_Media
            Atom_Packager
            Atom_Window
            Atom_Runtime
            Atom_UI
            Atom_ECS
            Atom_Physics
            Atom_Log
            Atom_Lua
            Atom_UTF8
            Atom_FS
            Atom_Assets
            Atom_Renderer2D
            Atom_Render_Resources
            Atom_Image
    )

    # Compatibility only. Keep this target while external consumers migrate;
    # do not add new dependencies or new users of Atom_Core.
    add_library(Atom_Core INTERFACE)
    target_link_libraries(Atom_Core INTERFACE Atom_All)

# 13. Lua Binding
    add_library(Atom_Lua STATIC
            Lua/lua.hpp
            Lua/LuaContext.hpp
            Lua/LuaGuard.hpp
            Lua/BindingRegistry.hpp
            Lua/LuaHost.hpp
            Lua/LuaHost.cpp
            Lua/LuaBinding/ContextBridge.hpp
            Lua/LuaBinding/LuaMusic.cpp
            Lua/LuaBinding/LuaSFX.cpp
            Lua/LuaBinding/LuaVolume.cpp
            Lua/LuaBinding/LuaLog.cpp
    )
    target_include_directories(Atom_Lua
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Lua)
    target_link_libraries(Atom_Lua PUBLIC
            Atom_3rd_Lua
            Atom_Log
            Atom_Media
            Atom_FS)

# ============================================================================
# Executable
# ============================================================================
# Main Exec
add_executable(Atom_Engine Atom.cpp)
set_target_properties(Atom_Engine PROPERTIES OUTPUT_NAME Atom)

target_link_libraries(Atom_Engine PRIVATE
        Atom_All
)

# Examples
add_executable(Example_Audio_Playback Example/Media/AudioPlayback.cpp)
target_link_libraries(Example_Audio_Playback PRIVATE
        Atom_Media Atom_Runtime Atom_Debugger)

add_executable(Example_SFX_Playback Example/Media/SFXPlayback.cpp)
target_link_libraries(Example_SFX_Playback PRIVATE
        Atom_Media Atom_Runtime Atom_Debugger)

add_executable(Example_Simple_Window Example/Window/SimpleWindow.cpp)
target_link_libraries(Example_Simple_Window PRIVATE Atom_Runtime)

add_executable(Example_Simple_Window_Debug Example/Window/SimpleWindowWithDebugger.cpp)
target_link_libraries(Example_Simple_Window_Debug PRIVATE Atom_Runtime Atom_Debugger)

# Audio metadata example (uses the engine's AudioMetadataReader)
add_executable(Example_Audio_Metadata Example/Media/AudioMetadata.cpp)
target_link_libraries(Example_Audio_Metadata PRIVATE Atom_Media Atom_Concurrency)

# Job system tutorial — a self-contained walkthrough of the scheduler API.
add_executable(Example_Job_System_Tour Example/Concurrency/JobSystemTour.cpp)
target_link_libraries(Example_Job_System_Tour PRIVATE Atom_Concurrency)

# Resource-pack playback example
add_executable(Example_Packaged_Music Example/Media/PackagedMusicPlayback.cpp)
target_link_libraries(Example_Packaged_Music PRIVATE
        Atom_Media Atom_Runtime Atom_Debugger Atom_Packager)

# Log channel
add_executable(Example_Log_Channel_Demo Example/Log/LogChannelDemo.cpp Example/Log/GameChannels.hpp)
target_link_libraries(Example_Log_Channel_Demo PRIVATE Atom_Log)

# Lua scripting example (loads an external script and plays a track via Lua)
add_executable(Example_Lua_Scripting Example/Lua/LuaScripting.cpp)
target_link_libraries(Example_Lua_Scripting PRIVATE
        Atom_Lua Atom_Media Atom_Log Atom_Runtime Atom_Debugger)

# MusicCard example — Yoga music HUD card migrated onto Renderer2D (phase C):
# cover art decode via Atom_Image, production drawing/text via Renderer2D,
# and an optional ImGui overlay for debug controls.
add_executable(Example_MusicCard
    Example/MusicCard/MusicCard.cpp
    Example/MusicCard/MusicCardScreen.cpp
    Example/MusicCard/MusicCardTypes.hpp
    Example/MusicCard/MusicCardScreen.hpp
    Example/MusicCard/MusicCardDebugger.hpp
)
target_link_libraries(Example_MusicCard PRIVATE
    Atom_Media Atom_Runtime Atom_Debugger Atom_Layout Atom_Renderer2D Atom_Render_Resources Atom_Image)
target_compile_definitions(Example_MusicCard PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_MusicCard Atom_Shaders)
# Resource telemetry reads the process working set via the Win32 PSAPI.
if(WIN32)
    target_link_libraries(Example_MusicCard PRIVATE psapi)
endif()

# T1.3 — the first screen that owns a GameWorld: hard-coded world, real res://
# sprites through Scene2D, keyboard-driven player, following camera. This is the
# example that T2.x replaces with a loaded scene, so it links only what it uses.
add_executable(Example_2D_Smoke
    Example/Game/SmokeMain.cpp
    Example/Game/SmokeScreen.cpp
    Example/Game/SmokeScreen.hpp
)
target_link_libraries(Example_2D_Smoke PRIVATE
    Atom_Runtime Atom_Scene Atom_Scene2D Atom_Render_Resources Atom_Renderer2D Atom_FS Atom_InputSystem Atom_Debugger)
target_compile_definitions(Example_2D_Smoke PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}"
    # Development content root (G0.5): Assets/ in the checkout is mounted as res://
    # at boot. A packaged build mounts the same URIs from an APKG instead.
    ATOM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
add_dependencies(Example_2D_Smoke Atom_Shaders)

# CORE-002 acceptance example — a box moved by the per-frame InputSystem, plus
# an overlay that prints the raw state. Proves the main loop feeds the input
# state machine, so the screen never touches SDL or Window events itself.
add_executable(Example_Input_Demo Example/Input/InputDemo.cpp)
target_link_libraries(Example_Input_Demo PRIVATE Atom_Runtime Atom_Debugger Atom_Renderer2D)
target_compile_definitions(Example_Input_Demo PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_Input_Demo Atom_Shaders)

# Retained game-UI acceptance example. It uses Runtime's ScreenManager for a
# menu/settings/game/pause flow; UI only owns widgets inside each page.
add_executable(Example_UI_Runtime_Demo Example/UI/UiRuntimeDemo.cpp)
target_link_libraries(Example_UI_Runtime_Demo PRIVATE Atom_Runtime Atom_UI)
target_compile_definitions(Example_UI_Runtime_Demo PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_UI_Runtime_Demo Atom_Shaders)

# Physics demo — interactive Renderer2D visualization of the Internal physics
# backend: gravity, stacking, a pendulum, a revolute chain, and click-to-spawn.
add_executable(Example_Physics_Demo Example/Physics/PhysicsDemo.cpp)
target_link_libraries(Example_Physics_Demo PRIVATE
    Atom_Physics Atom_Runtime Atom_Renderer2D Atom_Debugger)
target_compile_definitions(Example_Physics_Demo PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_Physics_Demo Atom_Shaders)

# Headless baseline for the physics JobSystem profiling gate. It intentionally
# has no renderer/window dependency so measurements isolate Step().
add_executable(Example_Physics_Benchmark Example/Physics/PhysicsBenchmark.cpp)
target_link_libraries(Example_Physics_Benchmark PRIVATE Atom_Physics Atom_Concurrency)

# ECS tour — a headless console walk through the entity/component store, ending in
# a real integration loop. Console rather than windowed on purpose: the ECS is pure
# storage, so the module can be exercised without a GPU, and this example doubles as
# the template for the first physics system.
add_executable(Example_ECS_Tour Example/ECS/EntityWorldTour.cpp)
target_link_libraries(Example_ECS_Tour PRIVATE Atom_ECS)

# ============================================================================
# Tools
# ============================================================================
add_executable(Atom_Tool_Packager
        Utilities/Packager/Package_main.cpp
)

add_executable(Example_Spatial_Backend_Probe EXCLUDE_FROM_ALL Example/Audio/SpatialBackendProbe.cpp)
target_link_libraries(Example_Spatial_Backend_Probe PRIVATE Atom_Media Atom_Runtime Atom_Debugger)
set_target_properties(Atom_Tool_Packager PROPERTIES OUTPUT_NAME packager_tool)
target_link_libraries(Atom_Tool_Packager PRIVATE Atom_Packager)

# Standalone ImGui packager tool (browse existing packages + build v2 packages).
add_executable(Atom_Tool_PackagerGui
        Utilities/Packager/PackagerGui.cpp
)
set_target_properties(Atom_Tool_PackagerGui PROPERTIES OUTPUT_NAME packager_gui)
target_link_libraries(Atom_Tool_PackagerGui PRIVATE Atom_Runtime Atom_Debugger Atom_Packager Atom_FS)

include(CTest)
# Third-party projects may force the generic BUILD_TESTING cache option off.
# Atom-owned tests use an independent option so that project coverage is not
# silently disabled by a dependency's configuration policy.
option(ATOM_BUILD_TESTS "Build Atom-owned test targets" ON)
if(ATOM_BUILD_TESTS)
    enable_testing()
    # All Atom-owned unit tests live under Test/<Module>/ and register themselves
    # via Test/CMakeLists.txt (CTest names: Atom_<Module>.<Area>).
    add_subdirectory(Test)
endif()

# ============================================================================
# Platforms Config
# ============================================================================
if(MINGW)
    target_link_options(Atom_Engine PRIVATE -lstdc++exp -lws2_32)

    # The thread runtime stays a DLL. The MinGW block near the top of this file
    # places every Atom executable, tests included, in CMAKE_BINARY_DIR, so copy
    # the DLL once into that directory instead of attaching a POST_BUILD copy to
    # every executable. Per-target POST_BUILD copies race under Ninja when
    # several executables link in parallel and can fail with ERROR_ACCESS_DENIED.
    get_filename_component(ATOM_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(ATOM_MINGW_RUNTIME_DLL "${ATOM_MINGW_BIN_DIR}/libwinpthread-1.dll")
    if(EXISTS "${ATOM_MINGW_RUNTIME_DLL}")
        set(ATOM_MINGW_RUNTIME_DLL_OUTPUT
            "${CMAKE_BINARY_DIR}/libwinpthread-1.dll")
        add_custom_command(
            OUTPUT "${ATOM_MINGW_RUNTIME_DLL_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ATOM_MINGW_RUNTIME_DLL}"
                    "${ATOM_MINGW_RUNTIME_DLL_OUTPUT}"
            DEPENDS "${ATOM_MINGW_RUNTIME_DLL}"
            COMMENT "Copying libwinpthread-1.dll to the runtime output directory"
            VERBATIM)
        add_custom_target(Atom_Mingw_Runtime_DLL
            DEPENDS "${ATOM_MINGW_RUNTIME_DLL_OUTPUT}")
        foreach(atom_exe IN ITEMS Atom_Engine Example_Audio_Playback
                               Example_Audio_Metadata Example_SFX_Playback
                               Example_Simple_Window Example_Simple_Window_Debug
                               Example_Packaged_Music Example_Log_Channel_Demo
                                Example_MusicCard Example_Lua_Scripting Example_Job_System_Tour
                                Example_Input_Demo Example_UI_Runtime_Demo Example_ECS_Tour Example_Physics_Demo Example_Physics_Benchmark
                               Atom_Tool_Packager
                               Atom_Tool_PackagerGui)
            add_dependencies(${atom_exe} Atom_Mingw_Runtime_DLL)
        endforeach()
        if(ATOM_BUILD_TESTS)
            get_property(atom_test_targets GLOBAL PROPERTY ATOM_TEST_TARGETS)
            foreach(atom_test IN LISTS atom_test_targets)
                if(TARGET ${atom_test})
                    add_dependencies(${atom_test} Atom_Mingw_Runtime_DLL)
                endif()
            endforeach()
        endif()
    endif()
endif()
if(WIN32)
    target_link_libraries(Atom_Engine PRIVATE opengl32 gdi32)
endif()


