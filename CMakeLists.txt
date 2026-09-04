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
# Engine Libraries
# ============================================================================
# 0. Interfaces (header-only, no SFML/SDL3 dependency)
    add_library(Atom_Backend_Contracts INTERFACE)
    target_sources(Atom_Backend_Contracts INTERFACE
        Backend/Contracts/Render/RenderTypes.hpp
        Backend/Contracts/Render/IRenderDevice.hpp
        Backend/Contracts/Render/IRender2DContext.hpp
        Backend/Contracts/Render/IRenderBackend.hpp
    )
    target_include_directories(Atom_Backend_Contracts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

# 1. Log
    add_library(Atom_Log STATIC
            Log/LogSystem.cpp
            Log/LogSystem.hpp
            Log/LogChannelMacros.hpp
            Log/AtomLogChannels.hpp
    )
    target_include_directories(Atom_Log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Log PUBLIC Atom_Backend_Contracts)

# 2. Algorithm
    add_library(Atom_Algorithm STATIC
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
            Algorithm/Transform/Transform2D.cpp
            Algorithm/Transform/Transform2D.hpp
            Algorithm/Transform/Transform3D.cpp
            Algorithm/Transform/Transform3D.hpp
            Algorithm/Math/Scalar.hpp
            Algorithm/Geometry/Primitives.hpp
            Algorithm/Camera/Camera2D.cpp
            Algorithm/Camera/Camera2D.hpp
            Algorithm/Camera/Camera3D.cpp
            Algorithm/Camera/Camera3D.hpp
    )
    target_include_directories(Atom_Algorithm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Algorithm PUBLIC Atom_Backend_Contracts)

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

# 3. SDL3 Core
    add_library(Atom_Backend_SDL3_Core STATIC
        Backend/SDL3/Core/SDLRuntime.cpp
        Backend/SDL3/Core/SDLRuntime.hpp
    )
    target_include_directories(Atom_Backend_SDL3_Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_SDL3_Core PUBLIC Atom_3rd_SDL3)

# 3a. SDL3 window platform layer
    add_library(Atom_Backend_SDL3_Window STATIC
        Backend/SDL3/Window/SDL3Window.cpp
        Backend/SDL3/Window/SDL3Window.hpp
        Backend/SDL3/Window/ISDL3WindowExtensions.hpp
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
    add_library(Atom_Backend_Render_Runtime STATIC
        Backend/Runtime/RenderBackendRuntime.cpp
        Backend/Runtime/RenderBackendRuntime.hpp
    )
    target_include_directories(Atom_Backend_Render_Runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_Render_Runtime PUBLIC Atom_Backend_Registry Atom_Backend_SDL_GPU)
    target_link_libraries(Atom_Backend_Render_Runtime PRIVATE Atom_Backend_SDLGPU_Debug)

# 3e. Text — backend-neutral font provider and glyph rasterization service.
    add_library(Atom_Text STATIC
        Render/Text/Font.cpp
        Render/Text/Font.hpp
    )
    target_include_directories(Atom_Text PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Text PRIVATE Atom_3rd_Stb)

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

# 4. Backend registry and engine audio decoders
    add_library(Atom_Backend_Registry STATIC
        Backend/Registry/AudioDecoderRegistry.hpp
        Backend/Registry/AudioDecoderRegistry.cpp
        Backend/Registry/BackendRegistry.hpp
        Backend/Registry/BackendRegistry.cpp
        Backend/Registry/RenderBackendRegistry.hpp
        Backend/Registry/RenderBackendRegistry.cpp
        Backend/Registry/DebugImGuiBackendRegistry.hpp
        Backend/Registry/DebugImGuiBackendRegistry.cpp
    )
    target_include_directories(Atom_Backend_Registry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_Registry PUBLIC Atom_Backend_Contracts)

    add_library(Atom_Backend_Audio STATIC
        Backend/Audio/Decoder/WavProf/RiffWaveReader.hpp
        Backend/Audio/Decoder/WavProf/RiffWaveReader.cpp
        Backend/Audio/Decoder/WavProf/WavProfDecoder.hpp
        Backend/Audio/Decoder/WavProf/WavProfDecoder.cpp
        Backend/Audio/Decoder/minimp3/Minimp3Decoder.hpp
        Backend/Audio/Decoder/minimp3/Minimp3Decoder.cpp
    )
    target_include_directories(Atom_Backend_Audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_Audio PUBLIC Atom_Backend_Contracts Atom_Backend_Registry Atom_3rd_Minimp3 Atom_UTF8)

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
    target_link_libraries(Atom_Backend_SDL3_Audio PUBLIC Atom_Backend_Contracts Atom_Backend_Registry Atom_Backend_SDL3_Core Atom_Log)

    add_library(Atom_Backend_Runtime STATIC
        Backend/Runtime/BackendRuntime.hpp
        Backend/Runtime/BackendRuntime.cpp
        Backend/Runtime/IAudioBackendChangeListener.hpp
    )
    target_include_directories(Atom_Backend_Runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_Runtime PUBLIC
        Atom_Backend_Contracts
        Atom_Backend_Registry
        Atom_Backend_Audio
        Atom_Backend_SDL3_Audio)

# 6. Entities
    add_library(Atom_Entities INTERFACE
            ECS/Components/Entities/Entity.hpp
            ECS/Components/Entities/NPC.hpp
            ECS/Components/Entities/Player.hpp
    )
    target_include_directories(Atom_Entities INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Entities INTERFACE Atom_Backend_Contracts Atom_Algorithm)

# 7. Media
    add_library(Atom_Media STATIC
        Media/Audio/Metadata/AudioMetadataReader.cpp
        Media/Audio/Metadata/AudioMetadataReader.hpp
        Media/Audio/Mixing/AudioMixer.cpp
        Media/Audio/Mixing/AudioMixer.hpp
        Media/Audio/Resources/AudioClipLoader.cpp
        Media/Audio/Resources/AudioClipLoader.hpp
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
        Media/Video/Video.cpp
        Media/Video/Video.hpp
    )
    target_include_directories(Atom_Media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Media PUBLIC Atom_Backend_Contracts Atom_Backend_Registry Atom_Backend_Runtime Atom_Log Atom_UTF8 Atom_3rd_TagLib)

# 8. UTF-8 encoding utilities
add_library(Atom_UTF8 STATIC
    Utilities/Utf8/Utf8.hpp
    Utilities/Utf8/Utf8.cpp
)
target_include_directories(Atom_UTF8 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Atom_UTF8 PUBLIC Atom_Backend_Contracts Atom_3rd_UtfCpp)

# 8a. Image decoding (CPU-side; GPU upload is a separate step via Renderer2D).
add_library(Atom_Image STATIC
    Media/Image/ImageDecoder.hpp
    Media/Image/ImageDecoder.cpp
)
target_include_directories(Atom_Image PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Atom_Image PRIVATE Atom_3rd_Stb Atom_Log)

# 9. Packager
    add_library(Atom_Packager STATIC
            Utilities/Packager/Unpackager.cpp
            Utilities/Packager/Unpackager.hpp
            Utilities/Packager/Packager.cpp
            Utilities/Packager/Packager.hpp
    )
    target_include_directories(Atom_Packager PUBLIC Utilities/Packager)
    target_link_libraries(Atom_Packager PUBLIC Atom_UTF8 Atom_Log)

# 10. Windows
    add_library(Atom_Window STATIC
            Window/RenderWindow.hpp
            Window/RenderWindow.cpp
            Window/Screen.cpp
            Window/Screen.hpp
            Window/Manager/ScreenManager.cpp
            Window/Manager/ScreenManager.hpp
            Window/OverlayManager.hpp
            Window/OverlayManager.cpp
    )
    target_include_directories(Atom_Window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Window PUBLIC
        Atom_Backend_Contracts
        Atom_Backend_Registry
        Atom_Backend_Render_Runtime
        Atom_Log
        Atom_Algorithm)

# 11. Debugger
    add_library(Atom_Debugger STATIC
            Window/Debugger.hpp
            Window/Debugger.cpp
            Window/LogDebugger.hpp
            Window/LogDebugger.cpp
            Window/Debugger/ImGui/ImGuiFontLoader.hpp
            Window/Debugger/ImGui/ImGuiFontLoader.cpp
            Window/Overlay.hpp
    )
    target_include_directories(Atom_Debugger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Debugger PUBLIC Atom_Window Atom_3rd_ImGui_SDL3 Atom_Backend_SDLGPU_Debug)
    if(WIN32)
        target_link_libraries(Atom_Debugger PUBLIC opengl32 gdi32)
    endif()

# 12. Config
    add_library(Atom_Config STATIC
            Config/Components/Entities/NPC.hpp
            Config/Movement/MoveEvent.hpp
    )
    target_link_libraries(Atom_Config PUBLIC Atom_Entities Atom_Log)

# 13. Engine Core
    add_library(Atom_Core INTERFACE)
    target_include_directories(Atom_Core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Core INTERFACE
            Atom_Algorithm
            Atom_Layout
            Atom_Entities
            Atom_Media
            Atom_Packager
            Atom_Window
            Atom_Log
            Atom_Lua
            Atom_Config
            Atom_UTF8
            Atom_Renderer2D
            Atom_Image
    )

# 14. Lua Binding
    add_library(Atom_Lua STATIC
            Lua/LuaLoader.hpp
            Lua/LuaLoader.cpp
            Lua/LuaBinding/LuaMusic.cpp
            Lua/LuaBinding/LuaSFX.cpp
            Lua/LuaBinding/LuaEntity.cpp
            Lua/LuaBinding/LuaVolume.cpp
    )
    target_include_directories(Atom_Lua
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Lua)
    target_link_libraries(Atom_Lua PUBLIC
            Atom_3rd_Lua
            Atom_Log
            Atom_Config
            Atom_Media)

# ============================================================================
# Executable
# ============================================================================
# Main Exec
add_executable(Atom_Engine Atom.cpp)
set_target_properties(Atom_Engine PROPERTIES OUTPUT_NAME Atom)

target_link_libraries(Atom_Engine PRIVATE
        Atom_Core
)

# Examples
add_executable(Example_Audio_Playback Example/Media/AudioPlayback.cpp)
target_link_libraries(Example_Audio_Playback PRIVATE
        Atom_Media Atom_Window Atom_Debugger)

add_executable(Example_SFX_Playback Example/Media/SFXPlayback.cpp)
target_link_libraries(Example_SFX_Playback PRIVATE
        Atom_Media Atom_Window Atom_Debugger)

add_executable(Example_Simple_Window Example/Window/SimpleWindow.cpp)
target_link_libraries(Example_Simple_Window PRIVATE Atom_Window)

add_executable(Example_Simple_Window_Debug Example/Window/SimpleWindowWithDebugger.cpp)
target_link_libraries(Example_Simple_Window_Debug PRIVATE Atom_Window Atom_Debugger)

# Audio metadata example (uses the engine's AudioMetadataReader)
add_executable(Example_Audio_Metadata Example/Media/AudioMetadata.cpp)
target_link_libraries(Example_Audio_Metadata PRIVATE Atom_Media)

# Resource-pack playback example
add_executable(Example_Packaged_Music Example/Media/PackagedMusicPlayback.cpp)
target_link_libraries(Example_Packaged_Music PRIVATE
        Atom_Media Atom_Window Atom_Debugger Atom_Packager)

# Log channel
add_executable(Example_Log_Channel_Demo Example/Log/LogChannelDemo.cpp)
target_link_libraries(Example_Log_Channel_Demo PRIVATE Atom_Log)

add_executable(Example_SDL_GPU_Clear_Smoke
    Example/Render/SDLGPUClearSmoke.cpp
    Example/Render/SDLGPUValidationScene.cpp
    Example/Render/SDLGPUValidationScene.hpp)
target_link_libraries(Example_SDL_GPU_Clear_Smoke PRIVATE Atom_Backend_SDL_GPU Atom_Render_Core)
target_compile_definitions(Example_SDL_GPU_Clear_Smoke PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_SDL_GPU_Clear_Smoke Atom_Shaders)

# Renderer2D smoke scene (phase C): batched 2D primitives, decoded images,
# atlas source rectangles, clipping, layers, and camera pan/zoom.
add_executable(Example_Renderer2D_Smoke Example/Render/Renderer2DSmoke.cpp)
target_link_libraries(Example_Renderer2D_Smoke PRIVATE
    Atom_Renderer2D Atom_Backend_SDL_GPU Atom_Image)
target_compile_definitions(Example_Renderer2D_Smoke PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}"
    ATOM_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(Example_Renderer2D_Smoke Atom_Shaders)

# MusicCard example — Yoga music HUD card migrated onto Renderer2D (phase C):
# cover art decode via Atom_Image, production drawing/text via Renderer2D,
# and an optional ImGui overlay for debug controls.
add_executable(Example_MusicCard Example/MusicCard/MusicCard.cpp)
target_link_libraries(Example_MusicCard PRIVATE
    Atom_Media Atom_Window Atom_Debugger Atom_Layout Atom_Renderer2D Atom_Image)
target_compile_definitions(Example_MusicCard PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_MusicCard Atom_Shaders)

# ============================================================================
# Tools
# ============================================================================
add_executable(Atom_Tool_Packager
        Utilities/Packager/Package_main.cpp
)
set_target_properties(Atom_Tool_Packager PROPERTIES OUTPUT_NAME packager_tool)
target_link_libraries(Atom_Tool_Packager PRIVATE Atom_Packager)

# ============================================================================
# Platforms Config
# ============================================================================
if(MINGW)
    # Statically link libgcc and libstdc++ so executables do not require
    # libgcc_s_seh-1.dll / libstdc++-6.dll at runtime. Plain "-static" is
    # avoided: this toolchain's static winpthread archive is built for UCRT and
    # references __intrinsic_setjmpex, which the MSVCRT runtime sequence does
    # not provide (undefined reference at link time).
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    target_link_options(Atom_Engine PRIVATE -lstdc++exp -lws2_32)

    # The thread runtime stays dynamic. All executables currently use the common
    # CMAKE_RUNTIME_OUTPUT_DIRECTORY (the build directory), so copy the DLL once
    # into that directory instead of attaching a POST_BUILD copy to every
    # executable. Per-target POST_BUILD copies race under Ninja when several
    # executables link in parallel and can fail with ERROR_ACCESS_DENIED.
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
                               Example_SDL_GPU_Clear_Smoke Example_Renderer2D_Smoke
                               Example_MusicCard Atom_Tool_Packager)
            add_dependencies(${atom_exe} Atom_Mingw_Runtime_DLL)
        endforeach()
    endif()
endif()
if(WIN32)
    target_link_libraries(Atom_Engine PRIVATE opengl32 gdi32)
endif()


