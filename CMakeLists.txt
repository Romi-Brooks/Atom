# ============================================================================
# Global Settings
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(Atom LANGUAGES C CXX)

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
    add_library(atom_backend_contracts INTERFACE)
    target_include_directories(atom_backend_contracts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

# 1. Log
    add_library(engine_log STATIC
            Log/LogSystem.cpp
            Log/LogSystem.hpp
    )
    target_include_directories(engine_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_log PUBLIC atom_backend_contracts)

# 2. Algorithm
    add_library(engine_algorithm STATIC
            Algorithm/Vector/Vec2.cpp
            Algorithm/Vector/Vec2.hpp
            Algorithm/Vector/Vec3.cpp
            Algorithm/Vector/Vec3.hpp
    )
    target_include_directories(engine_algorithm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_algorithm PUBLIC atom_backend_contracts)

# 3. SDL3 Core
    add_library(atom_backend_sdl3_core STATIC
        Backend/SDL3/Core/SDLRuntime.cpp
        Backend/SDL3/Core/SDLRuntime.hpp
    )
    target_include_directories(atom_backend_sdl3_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(atom_backend_sdl3_core PUBLIC SDL3::SDL3)

# 3. SDL3 Render Backend
    add_library(atom_backend_sdl3_render STATIC
        Backend/SDL3/Window/SDL3RenderWindow.cpp
        Backend/SDL3/Window/SDL3RenderWindow.hpp
        Backend/SDL3/Render/SDL3Texture.cpp
        Backend/SDL3/Render/SDL3Texture.hpp
    )
    target_include_directories(atom_backend_sdl3_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(atom_backend_sdl3_render PUBLIC atom_backend_contracts atom_backend_sdl3_core)

# 4. Backend registry and builtin audio decoders
    add_library(atom_backend_registry STATIC
        Backend/Registry/AudioDecoderRegistry.hpp
        Backend/Registry/AudioDecoderRegistry.cpp
        Backend/Registry/BackendRegistry.hpp
        Backend/Registry/BackendRegistry.cpp
    )
    target_include_directories(atom_backend_registry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(atom_backend_registry PUBLIC atom_backend_contracts)

    add_library(atom_backend_builtin_audio STATIC
        Backend/Builtin/Audio/RegisterBuiltinAudio.hpp
        Backend/Builtin/Audio/RegisterBuiltinAudio.cpp
        Backend/Builtin/Audio/Decoder/WavRiff/RiffWaveReader.hpp
        Backend/Builtin/Audio/Decoder/WavRiff/RiffWaveReader.cpp
        Backend/Builtin/Audio/Decoder/WavRiff/BuiltinWavDecoder.hpp
        Backend/Builtin/Audio/Decoder/WavRiff/BuiltinWavDecoder.cpp
    )
    target_include_directories(atom_backend_builtin_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(atom_backend_builtin_audio PUBLIC atom_backend_contracts atom_backend_registry)

# 5. SDL3 Audio Backend
    add_library(atom_backend_sdl3_audio STATIC
        Backend/SDL3/Audio/SDL3AudioBackend.cpp
        Backend/SDL3/Audio/SDL3AudioBackend.hpp
        Backend/SDL3/Audio/RegisterSDL3Audio.cpp
        Backend/SDL3/Audio/RegisterSDL3Audio.hpp
        Backend/SDL3/Audio/Decoder/SDL3WavDecoder.cpp
        Backend/SDL3/Audio/Decoder/SDL3WavDecoder.hpp
        Backend/SDL3/Audio/SDL3SFXSource.cpp
        Backend/SDL3/Audio/SDL3SFXSource.hpp
        Backend/SDL3/Audio/SDL3MusicSource.cpp
        Backend/SDL3/Audio/SDL3MusicSource.hpp
        Backend/SDL3/Audio/SDL3StreamingMusicSource.cpp
        Backend/SDL3/Audio/SDL3StreamingMusicSource.hpp
    )
    target_include_directories(atom_backend_sdl3_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(atom_backend_sdl3_audio PUBLIC atom_backend_contracts atom_backend_registry atom_backend_sdl3_core engine_log)

    add_library(atom_backend_runtime STATIC
        Backend/Runtime/BackendRuntime.hpp
        Backend/Runtime/BackendRuntime.cpp
        Backend/Runtime/IAudioBackendChangeListener.hpp
    )
    target_include_directories(atom_backend_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(atom_backend_runtime PUBLIC
        atom_backend_contracts
        atom_backend_registry
        atom_backend_builtin_audio
        atom_backend_sdl3_audio)

# 6. Entities
    add_library(engine_entities STATIC
            Components/Entities/Entity.hpp
            Components/Entities/Entity.cpp
            Components/Entities/NPC.hpp
            Components/Entities/NPC.cpp
            Components/Entities/Player.hpp
            Components/Entities/Player.cpp
    )
    target_include_directories(engine_entities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_entities PUBLIC atom_backend_contracts engine_algorithm)

# 7. Media
    add_library(engine_media STATIC
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
    target_include_directories(engine_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_media PUBLIC atom_backend_contracts atom_backend_registry atom_backend_runtime engine_log)

# 8. Packager
    add_library(engine_packager STATIC
            Utilities/Packager/Unpackager.cpp
            Utilities/Packager/Unpackager.hpp
            Utilities/Packager/Packager.cpp
            Utilities/Packager/Packager.hpp
    )
    target_include_directories(engine_packager PUBLIC Utilities/Packager)
    target_link_libraries(engine_packager PUBLIC atom::utfcpp engine_log)

# 9. Windows
    add_library(engine_windows STATIC
            Window/RenderWindow.hpp
            Window/RenderWindow.cpp
            Window/Screen.cpp
            Window/Screen.hpp
            Window/Manager/ScreenManager.cpp
            Window/Manager/ScreenManager.hpp
    )
    target_include_directories(engine_windows PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_windows PUBLIC
        atom_backend_contracts
        atom_backend_sdl3_render
        engine_log
        engine_algorithm)

# 10. Debugger
    add_library(engine_debugger STATIC
            Window/Debugger.hpp
            Window/Debugger.cpp
    )
    target_include_directories(engine_debugger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_debugger PUBLIC engine_windows atom::imgui_sdl3)
    if(WIN32)
        target_link_libraries(engine_debugger PUBLIC opengl32 gdi32)
    endif()

# 11. Config
    add_library(engine_config STATIC
            Config/Components/Entities/NPC.hpp
            Config/Movement/MoveEvent.hpp
    )
    target_link_libraries(engine_config PUBLIC engine_entities engine_log)

# 12. Engine Core
    add_library(engine_core INTERFACE)
    target_include_directories(engine_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_core INTERFACE
            engine_algorithm
            engine_entities
            engine_media
            engine_packager
            engine_windows
            engine_log
            engine_lua
            engine_config
    )

# 13. Lua Binding
    add_library(engine_lua STATIC
            Lua/LuaLoader.hpp
            Lua/LuaLoader.cpp
            Lua/LuaBinding/LuaMusic.cpp
            Lua/LuaBinding/LuaSFX.cpp
            Lua/LuaBinding/LuaEntity.cpp
            Lua/LuaBinding/LuaVolume.cpp
    )
    target_include_directories(engine_lua
            PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Lua)
    target_link_libraries(engine_lua PUBLIC
            atom::lua
            engine_log
            engine_config
            engine_media)

# ============================================================================
# Executable
# ============================================================================
# Main Exec
add_executable(Atom Atom.cpp)

target_link_libraries(Atom PRIVATE
        engine_core
        atom::taglib
)

# Examples
add_executable(example_media_metadata Example/MediaMetadata.cpp)
target_link_libraries(example_media_metadata PRIVATE atom::taglib)
if(WIN32)
    target_link_libraries(example_media_metadata PRIVATE ws2_32 winmm)
endif()

add_executable(example_audio_playback Example/MusicPlayback.cpp)
target_link_libraries(example_audio_playback PRIVATE
        engine_media engine_windows engine_debugger)
if(WIN32)
    target_link_libraries(example_audio_playback PRIVATE)
endif()

add_executable(example_sfx_playback Example/SFXPlayback.cpp)
target_link_libraries(example_sfx_playback PRIVATE
        engine_media engine_windows engine_debugger)
if(WIN32)
    target_link_libraries(example_sfx_playback PRIVATE)
endif()

add_executable(example_simple_window Example/SimpleWindow.cpp)
target_link_libraries(example_simple_window PRIVATE engine_windows)
if(WIN32)
    target_link_libraries(example_simple_window PRIVATE)
endif()

add_executable(example_simple_window_debug Example/SimpleWindowWithDebugger.cpp)
target_link_libraries(example_simple_window_debug PRIVATE engine_windows engine_debugger)
if(WIN32)
    target_link_libraries(example_simple_window_debug PRIVATE)
endif()

# ============================================================================
# Tools
# ============================================================================
add_executable(packager_tool
        Utilities/Packager/Package_main.cpp
)
target_link_libraries(packager_tool PRIVATE engine_packager)

# ============================================================================
# Platforms Config
# ============================================================================
if(MINGW)
    target_link_options(Atom PRIVATE -lstdc++exp -lws2_32)
endif()
if(WIN32)
    target_link_libraries(Atom PRIVATE opengl32 gdi32)
endif()


