# ============================================================================
# Global Settings
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(Atom)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
if(WIN32)
    set(PLATFORM_SUFFIX "win64")
    set(DLL_EXT "dll")
    set(LIB_EXT "lib")
elseif(UNIX)
    set(PLATFORM_SUFFIX "linux")
    set(DLL_EXT "so")
    set(LIB_EXT "so")
elseif(APPLE)
    set(PLATFORM_SUFFIX "osx")
    set(DLL_EXT "dylib")
    set(LIB_EXT "dylib")
endif()

# ============================================================================
# 3rd Libraries Paths
# ============================================================================
set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/SDL3/x86_64-w64-mingw32/lib/cmake/SDL3")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/ImGUI")
set(LUA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lua")
set(TAGLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/taglib")
set(UTFCPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/utfcpp")
set(SDL3_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/SDL3/x86_64-w64-mingw32")

# ============================================================================
# 3rd Libraries Path Check
# ============================================================================
foreach(DIR IN ITEMS ${SDL3_ROOT} ${IMGUI_DIR} ${LUA_SRC_DIR}
        ${TAGLIB_DIR} ${UTFCPP_DIR})
    if(NOT EXISTS ${DIR})
        message(FATAL_ERROR "Required directory not found: ${DIR}")
    endif()
endforeach()

# ============================================================================
# 3rd Libraries Config
# ============================================================================
# Includes
include_directories(
        ${SDL3_ROOT}/include
        ${IMGUI_DIR}
        ${TAGLIB_DIR}/include
        ${UTFCPP_DIR}
)

# Link
link_directories(
        "${SDL3_ROOT}/lib"
        "${TAGLIB_DIR}/lib"
)

# 1. SDL3
    find_package(SDL3 REQUIRED CONFIG
        PATHS "${SDL3_DIR}"
        NO_DEFAULT_PATH)

# 2. Lua
    file(GLOB LUA_SOURCES
            ${LUA_SRC_DIR}/*.c
    )
    list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/lua\\.c$")
    list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/luac\\.c$")

    add_library(lua_lib STATIC ${LUA_SOURCES})
    target_include_directories(lua_lib PUBLIC ${LUA_SRC_DIR})

# 3. ImGui (core)
    add_library(imgui_lib STATIC
            "${IMGUI_DIR}/imgui.cpp"
            "${IMGUI_DIR}/imgui_draw.cpp"
            "${IMGUI_DIR}/imgui_tables.cpp"
            "${IMGUI_DIR}/imgui_widgets.cpp"
    )
    target_include_directories(imgui_lib PUBLIC ${IMGUI_DIR})

# 4. ImGui SDL3 backends
    add_library(imgui_sdl3_lib STATIC
            "${IMGUI_DIR}/imgui_impl_sdl3.cpp"
            "${IMGUI_DIR}/imgui_impl_sdlrenderer3.cpp"
    )
    target_include_directories(imgui_sdl3_lib PUBLIC ${IMGUI_DIR})
    target_link_libraries(imgui_sdl3_lib PUBLIC imgui_lib SDL3::SDL3)

# 5. TagLib
    set(TAGLIB_INCLUDE_DIR "${TAGLIB_DIR}/include")
    set(TAGLIB_LIB_FILE "${TAGLIB_DIR}/lib/libtag.a")

    add_library(taglib STATIC IMPORTED GLOBAL)
    set_target_properties(taglib PROPERTIES
            IMPORTED_LOCATION "${TAGLIB_LIB_FILE}"
            INTERFACE_INCLUDE_DIRECTORIES "${TAGLIB_INCLUDE_DIR}"
            INTERFACE_COMPILE_DEFINITIONS "TAGLIB_STATIC"
    )

# 6. utfcpp
    add_library(utfcpp INTERFACE)
    target_include_directories(utfcpp INTERFACE
            ${UTFCPP_DIR}
    )


# ============================================================================
# Engine Libraries
# ============================================================================
# 0. Interfaces (header-only, no SFML/SDL3 dependency)
    add_library(engine_interfaces INTERFACE)
    target_include_directories(engine_interfaces INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

# 1. Log
    add_library(engine_log STATIC
            Log/LogSystem.cpp
            Log/LogSystem.hpp
    )
    target_include_directories(engine_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_log PUBLIC engine_interfaces)

# 2. Algorithm
    add_library(engine_algorithm STATIC
            Algorithm/Vector/Vec2.cpp
            Algorithm/Vector/Vec2.hpp
    )
    target_include_directories(engine_algorithm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_algorithm PUBLIC engine_interfaces)

# 3. SDL3 Render Backend
    add_library(engine_render_sdl3 STATIC
        Engine/Render/SDLCore.cpp
        Engine/Render/SDLCore.hpp
        Engine/Render/SDL3RenderWindow.cpp
        Engine/Render/SDL3RenderWindow.hpp
        Engine/Render/SDL3Texture.cpp
        Engine/Render/SDL3Texture.hpp
    )
    target_include_directories(engine_render_sdl3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_render_sdl3 PUBLIC engine_interfaces SDL3::SDL3)

# 4. Audio Decoder Backends (builtin WAV RIFF decoder + IAudioDecoder implementations)
    add_library(engine_audio_decoder STATIC
        Engine/Interfaces/IAudioDecoder.hpp
        Engine/Audio/DecoderRegistry.hpp
        Engine/Audio/DecoderRegistry.cpp
        Media/Audio/Decoder/WavRiff/WavDecoder.cpp
        Media/Audio/Decoder/WavRiff/WavDecoder.hpp
        Media/Audio/Decoder/AtomWavDecoderBackend.cpp
        Media/Audio/Decoder/AtomWavDecoderBackend.hpp
    )
    target_include_directories(engine_audio_decoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_audio_decoder PUBLIC engine_interfaces engine_log)

# 5. SDL3 Audio Backend
    add_library(engine_audio_sdl3 STATIC
        Media/Audio/Backend/SDL3AudioBuffer.cpp
        Media/Audio/Backend/SDL3AudioBuffer.hpp
        Media/Audio/Backend/SDL3SFXSource.cpp
        Media/Audio/Backend/SDL3SFXSource.hpp
        Media/Audio/Backend/SDL3MusicSource.cpp
        Media/Audio/Backend/SDL3MusicSource.hpp
    )
    target_include_directories(engine_audio_sdl3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_audio_sdl3 PUBLIC engine_interfaces engine_log SDL3::SDL3)

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
    target_link_libraries(engine_entities PUBLIC engine_interfaces engine_algorithm)

# 7. Media
    add_library(engine_media STATIC
            Media/Audio/SFX/Manager/SFXManager.cpp
            Media/Audio/SFX/Manager/SFXManager.hpp
            Media/Audio/Manager/VolumeManager.cpp
            Media/Audio/Manager/VolumeManager.hpp
            Media/Audio/Plugs/MusicFade.cpp
            Media/Audio/Plugs/MusicFade.hpp
            Media/Audio/Music/Music.cpp
            Media/Audio/Music/Music.hpp
            Media/Audio/SFX/SFX.cpp
            Media/Audio/SFX/SFX.hpp
            Media/Video/Video.cpp
            Media/Video/Video.hpp
    )
    target_include_directories(engine_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_media PUBLIC engine_interfaces engine_log engine_audio_sdl3 engine_audio_decoder)

# 8. Packager
    add_library(engine_packager STATIC
            Utilities/Packager/Unpackager.cpp
            Utilities/Packager/Unpackager.hpp
            Utilities/Packager/Packager.cpp
            Utilities/Packager/Packager.hpp
    )
    target_include_directories(engine_packager PUBLIC Utilities/Packager)
    target_link_libraries(engine_packager PUBLIC utfcpp engine_log)

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
        engine_interfaces
        engine_render_sdl3
        engine_log
        engine_algorithm)

# 10. Debugger
    add_library(engine_debugger STATIC
            Window/Debugger.hpp
            Window/Debugger.cpp
    )
    target_include_directories(engine_debugger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_debugger PUBLIC engine_windows imgui_sdl3_lib)
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
    target_include_directories(engine_lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(engine_lua PUBLIC
            lua_lib
            engine_log
            engine_config)

# ============================================================================
# Executable
# ============================================================================
# Main Exec
add_executable(Atom Atom.cpp)

target_link_libraries(Atom PRIVATE
        engine_core
        taglib
)

# Examples
add_executable(example_media_metadata Example/MediaMetadata.cpp)
target_link_libraries(example_media_metadata PRIVATE taglib)
if(WIN32)
    target_link_libraries(example_media_metadata PRIVATE ws2_32 winmm)
endif()

add_executable(example_audio_playback Example/MusicPlayback.cpp)
target_link_libraries(example_audio_playback PRIVATE engine_media engine_windows engine_debugger)
if(WIN32)
    target_link_libraries(example_audio_playback PRIVATE)
endif()

add_executable(example_sfx_playback Example/SFXPlayback.cpp)
target_link_libraries(example_sfx_playback PRIVATE engine_media engine_windows engine_debugger)
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

# ============================================================================
# Post Build Config
# ============================================================================
# Copy SDL3 DLL to output directories
set(SDL3_BIN_DIR "${SDL3_ROOT}/bin")

if(EXISTS ${SDL3_BIN_DIR})
    file(GLOB SDL3_DLLS "${SDL3_BIN_DIR}/*.dll")
    if(SDL3_DLLS)
        foreach(TGT Atom example_simple_window example_simple_window_debug
                    example_audio_playback example_sfx_playback)
            add_custom_command(TARGET ${TGT} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${TGT}>"
                    COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_DLLS} "$<TARGET_FILE_DIR:${TGT}>"
                    COMMENT "Copying SDL3 DLLs to ${TGT} output directory"
            )
        endforeach()
        message(STATUS "SDL3 DLLs found: ${SDL3_DLLS}")
    else()
        message(WARNING "No SDL3 DLLs found in ${SDL3_BIN_DIR}")
    endif()
else()
    message(WARNING "SDL3 bin directory not found: ${SDL3_BIN_DIR}")
endif()


