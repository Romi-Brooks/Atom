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


# ============================================================================
# Engine Libraries
# ============================================================================
# 0. Interfaces (header-only, no SFML/SDL3 dependency)
    add_library(Atom_Backend_Contracts INTERFACE)
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
    )
    target_include_directories(Atom_Algorithm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Algorithm PUBLIC Atom_Backend_Contracts)

# 2b. Layout
    add_library(Atom_Layout STATIC
        Layout/LayoutConfig.cpp
        Layout/LayoutConfig.hpp
        Layout/LayoutNode.cpp
        Layout/LayoutNode.hpp
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

# 3. SDL3 Render Backend
    add_library(Atom_Backend_SDL3_Render STATIC
        Backend/SDL3/Window/SDL3RenderWindow.cpp
        Backend/SDL3/Window/SDL3RenderWindow.hpp
        Backend/SDL3/Window/ISDL3WindowExtensions.hpp
        Backend/SDL3/Render/SDL3Texture.cpp
        Backend/SDL3/Render/SDL3Texture.hpp
    )
    target_include_directories(Atom_Backend_SDL3_Render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    # SDL3RenderWindow.cpp calls Vec2 (GetX/GetY) directly, so Atom_Algorithm
    # must be an explicit dependency — never rely on consumer link ordering.
    target_link_libraries(Atom_Backend_SDL3_Render PUBLIC Atom_Backend_Contracts Atom_Backend_SDL3_Core Atom_Algorithm)

# 3b. SDL3 Debug (ImGui) Backend — ImGui glue for the SDL3 render backend.
    add_library(Atom_Backend_SDL3_Debug STATIC
        Backend/SDL3/Debug/SDL3DebugImGuiBackend.cpp
        Backend/SDL3/Debug/SDL3DebugImGuiBackend.hpp
    )
    target_include_directories(Atom_Backend_SDL3_Debug PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_SDL3_Debug PUBLIC Atom_Backend_Contracts Atom_Backend_SDL3_Render Atom_3rd_ImGui_SDL3)

# 3c. Render Backend Runtime — the sanctioned layer that knows the concrete
#     backends and registers them (window factory + debug overlay backend);
#     upper layers only consume the registries and the I* interfaces.
    add_library(Atom_Backend_Render_Runtime STATIC
        Backend/Runtime/RenderBackendRuntime.cpp
        Backend/Runtime/RenderBackendRuntime.hpp
    )
    target_include_directories(Atom_Backend_Render_Runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_Render_Runtime PUBLIC Atom_Backend_Registry Atom_Backend_SDL3_Render Atom_Backend_SDL3_Debug)

# 4. Backend registry and builtin audio decoders
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

    add_library(Atom_Backend_Builtin_Audio STATIC
        Backend/Builtin/Audio/Decoder/WavProf/RiffWaveReader.hpp
        Backend/Builtin/Audio/Decoder/WavProf/RiffWaveReader.cpp
        Backend/Builtin/Audio/Decoder/WavProf/WavProfDecoder.hpp
        Backend/Builtin/Audio/Decoder/WavProf/WavProfDecoder.cpp
        Backend/Builtin/Audio/Decoder/minimp3/Minimp3Decoder.hpp
        Backend/Builtin/Audio/Decoder/minimp3/Minimp3Decoder.cpp
    )
    target_include_directories(Atom_Backend_Builtin_Audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Backend_Builtin_Audio PUBLIC Atom_Backend_Contracts Atom_Backend_Registry Atom_3rd_Minimp3 Atom_UTF8)

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
        Atom_Backend_Builtin_Audio
        Atom_Backend_SDL3_Audio)

# 6. Entities
    add_library(Atom_Entities STATIC
            ECS/Components/Entities/Entity.hpp
            ECS/Components/Entities/Entity.cpp
            ECS/Components/Entities/NPC.hpp
            ECS/Components/Entities/NPC.cpp
            ECS/Components/Entities/Player.hpp
            ECS/Components/Entities/Player.cpp
    )
    target_include_directories(Atom_Entities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Entities PUBLIC Atom_Backend_Contracts Atom_Algorithm)

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

# --------------------------------------------------------------------------
# UTF-8 encoding utilities (shared by all Atom modules)
# --------------------------------------------------------------------------
add_library(Atom_UTF8 STATIC
    Utilities/Utf8/Utf8.hpp
    Utilities/Utf8/Utf8.cpp
)
target_include_directories(Atom_UTF8 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Atom_UTF8 PUBLIC Atom_Backend_Contracts Atom_3rd_UtfCpp)

# 8. Packager
    add_library(Atom_Packager STATIC
            Utilities/Packager/Unpackager.cpp
            Utilities/Packager/Unpackager.hpp
            Utilities/Packager/Packager.cpp
            Utilities/Packager/Packager.hpp
    )
    target_include_directories(Atom_Packager PUBLIC Utilities/Packager)
    target_link_libraries(Atom_Packager PUBLIC Atom_3rd_UtfCpp Atom_Log)

# 9. Windows
    add_library(Atom_Window STATIC
            Window/RenderWindow.hpp
            Window/RenderWindow.cpp
            Window/Screen.cpp
            Window/Screen.hpp
            Window/Manager/ScreenManager.cpp
            Window/Manager/ScreenManager.hpp
    )
    target_include_directories(Atom_Window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Window PUBLIC
        Atom_Backend_Contracts
        Atom_Backend_Registry
        Atom_Backend_Render_Runtime
        Atom_Log
        Atom_Algorithm)

# 10. Debugger
    add_library(Atom_Debugger STATIC
            Window/Debugger.hpp
            Window/Debugger.cpp
            Window/Debugger/ImGui/ImGuiFontLoader.hpp
            Window/Debugger/ImGui/ImGuiFontLoader.cpp
            Window/Overlay.hpp
    )
    target_include_directories(Atom_Debugger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Atom_Debugger PUBLIC Atom_Window Atom_Backend_SDL3_Debug)
    if(WIN32)
        target_link_libraries(Atom_Debugger PUBLIC opengl32 gdi32)
    endif()

# 11. Config
    add_library(Atom_Config STATIC
            Config/Components/Entities/NPC.hpp
            Config/Movement/MoveEvent.hpp
    )
    target_link_libraries(Atom_Config PUBLIC Atom_Entities Atom_Log)

# 12. Engine Core
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
    )

# 13. Lua Binding
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
if(WIN32)
    target_link_libraries(Example_Audio_Playback PRIVATE)
endif()

add_executable(Example_SFX_Playback Example/Media/SFXPlayback.cpp)
target_link_libraries(Example_SFX_Playback PRIVATE
        Atom_Media Atom_Window Atom_Debugger)
if(WIN32)
    target_link_libraries(Example_SFX_Playback PRIVATE)
endif()

add_executable(Example_Simple_Window Example/Window/SimpleWindow.cpp)
target_link_libraries(Example_Simple_Window PRIVATE Atom_Window)
if(WIN32)
    target_link_libraries(Example_Simple_Window PRIVATE)
endif()

add_executable(Example_Simple_Window_Debug Example/Window/SimpleWindowWithDebugger.cpp)
target_link_libraries(Example_Simple_Window_Debug PRIVATE Atom_Window Atom_Debugger)
if(WIN32)
    target_link_libraries(Example_Simple_Window_Debug PRIVATE)
endif()

add_executable(Example_Flex_Layout Example/Layout/FlexLayout.cpp)
target_link_libraries(Example_Flex_Layout PRIVATE Atom_Layout)

add_executable(Example_Music_Card_Layout Example/Layout/MusicCard.cpp)
target_link_libraries(Example_Music_Card_Layout PRIVATE
        Atom_Layout Atom_Media Atom_Window Atom_Debugger Atom_Backend_SDL3_Render)
if(WIN32)
    target_link_libraries(Example_Music_Card_Layout PRIVATE windowscodecs ole32)
endif()

# Audio metadata example (uses the engine's AudioMetadataReader)
add_executable(Example_Audio_Metadata Example/Media/AudioMetadata.cpp)
target_link_libraries(Example_Audio_Metadata PRIVATE Atom_Media)
if(WIN32)
    target_link_libraries(Example_Audio_Metadata PRIVATE)
endif()

# Resource-pack + in-memory streaming playback example
add_executable(Example_Packaged_Music Example/Media/PackagedMusicPlayback.cpp)
target_link_libraries(Example_Packaged_Music PRIVATE
        Atom_Media Atom_Window Atom_Debugger Atom_Packager)
if(WIN32)
    target_link_libraries(Example_Packaged_Music PRIVATE)
endif()

# Log channel demo (engine channels + game-side registration test)
add_executable(Example_Log_Channel_Demo Example/Log/LogChannelDemo.cpp)
target_link_libraries(Example_Log_Channel_Demo PRIVATE Atom_Log)
if(WIN32)
    target_link_libraries(Example_Log_Channel_Demo PRIVATE)
endif()

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

    # The thread runtime stays dynamic; copy libwinpthread-1.dll next to every
    # executable so they run without manual DLL setup.
    get_filename_component(ATOM_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(ATOM_MINGW_RUNTIME_DLL "${ATOM_MINGW_BIN_DIR}/libwinpthread-1.dll")
    function(atom_copy_mingw_runtime_dll target)
        if(EXISTS "${ATOM_MINGW_RUNTIME_DLL}")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ATOM_MINGW_RUNTIME_DLL}" "$<TARGET_FILE_DIR:${target}>"
                COMMENT "Copying libwinpthread-1.dll next to ${target}")
        endif()
    endfunction()
    foreach(atom_exe IN ITEMS Atom_Engine Example_Audio_Playback
                           Example_Audio_Metadata Example_SFX_Playback
                           Example_Simple_Window Example_Simple_Window_Debug
                           Example_Flex_Layout Example_Music_Card_Layout
                           Example_Packaged_Music Example_Log_Channel_Demo
                           Atom_Tool_Packager)
        atom_copy_mingw_runtime_dll(${atom_exe})
    endforeach()
endif()
if(WIN32)
    target_link_libraries(Atom_Engine PRIVATE opengl32 gdi32)
endif()


