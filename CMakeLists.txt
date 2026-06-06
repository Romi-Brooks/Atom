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
# 3rd Libraries
# ============================================================================
set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/SFML-3.0.0")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/ImGUI")
set(IMGUI_SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/ImGUI-SFML")
set(LUA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/Lua")
set(TAGLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/taglib")
set(UTFCPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/utfcpp")
set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/FFmpeg")

# ============================================================================
# 3rd Libraries Path Check
# ============================================================================
foreach(DIR IN ITEMS ${SFML_DIR} ${IMGUI_DIR} ${IMGUI_SFML_DIR} ${LUA_SRC_DIR}
        ${TAGLIB_DIR} ${UTFCPP_DIR} ${FFMPEG_DIR})
    if(NOT EXISTS ${DIR})
        message(FATAL_ERROR "Required directory not found: ${DIR}")
    endif()
endforeach()

# ============================================================================
# 3rd Libraries Config
# ============================================================================
# Includes
include_directories(
        ${SFML_DIR}/include
        ${IMGUI_DIR}
        ${IMGUI_SFML_DIR}
        ${TAGLIB_DIR}/include
        ${UTFCPP_DIR}
        ${FFMPEG_DIR}/include
)

# Link
link_directories(
        "${SFML_DIR}/lib"
        "${TAGLIB_DIR}/lib"
        "${FFMPEG_DIR}/lib"
)

# 1. SFML
    find_package(SFML 3.0 REQUIRED COMPONENTS Graphics Window System Audio
            PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/SFML-3.0.0"
            NO_DEFAULT_PATH)

    add_library(sfml_all INTERFACE)
    target_link_libraries(sfml_all INTERFACE
            sfml-graphics
            sfml-window
            sfml-system
            sfml-audio
    )

# 2. Lua
    file(GLOB LUA_SOURCES
            ${LUA_SRC_DIR}/*.c
    )
    list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/lua\\.c$")
    list(FILTER LUA_SOURCES EXCLUDE REGEX ".*/luac\\.c$")

    add_library(lua_lib STATIC ${LUA_SOURCES})
    target_include_directories(lua_lib PUBLIC ${LUA_SRC_DIR})

# 3. ImGui
    add_library(imgui_lib STATIC
            "${IMGUI_DIR}/imgui.cpp"
            "${IMGUI_DIR}/imgui_draw.cpp"
            "${IMGUI_DIR}/imgui_tables.cpp"
            "${IMGUI_DIR}/imgui_widgets.cpp"
    )
    target_include_directories(imgui_lib PUBLIC ${IMGUI_DIR})

# 4. ImGui-SFML
    add_library(imgui_sfml_lib STATIC
            "${IMGUI_SFML_DIR}/imgui-SFML.cpp"
    )
    target_include_directories(imgui_sfml_lib PUBLIC ${IMGUI_SFML_DIR})
    target_link_libraries(imgui_sfml_lib PUBLIC imgui_lib sfml_all)

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

# 7. FFmpeg
    add_library(ffmpeg INTERFACE)
    target_link_libraries(ffmpeg INTERFACE
            avcodec
            avformat
            avutil
            swscale
            swresample
            avfilter
    )

# ============================================================================
# Engine Libraries
# ============================================================================
# 1. Log
add_library(engine_log STATIC
        Log/LogSystem.cpp
        Log/LogSystem.hpp
)
target_include_directories(engine_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Dependency Libraries: None

# 2. Math
add_library(engine_math STATIC
        Math/Vector/Vec2.cpp
        Math/Vector/Vec2.hpp
)
target_include_directories(engine_math PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Dependency Libraries: None

# 3. Entities
add_library(engine_entities STATIC
        Components/Entities/Entity.cpp
        Components/Entities/Entity.hpp
        Components/Entities/NPC.cpp
        Components/Entities/NPC.hpp
        Components/Entities/Player.cpp
        Components/Entities/Player.hpp
)
target_include_directories(engine_entities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Dependency Libraries: None

# 4. Media
add_library(engine_media STATIC
        Media/Audio/Manager/SFXManager.cpp
        Media/Audio/Manager/SFXManager.hpp
        Media/Audio/Manager/VolumeManager.cpp
        Media/Audio/Manager/VolumeManager.hpp
        Media/Audio/Plug/MusicFade.cpp
        Media/Audio/Plug/MusicFade.hpp
        Media/Audio/Music.cpp
        Media/Audio/Music.hpp
        Media/Audio/SFX.cpp
        Media/Audio/SFX.hpp
        Media/Decoder/WavRiff/WavDecoder.cpp
        Media/Decoder/WavRiff/WavDecoder.hpp
        Media/Video/Video.cpp
        Media/Video/Video.hpp
)
target_include_directories(engine_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Dependency Libraries: SFML, Engine.Log
target_link_libraries(engine_media PUBLIC sfml_all engine_log)

## 5. FileSystem
#add_library(engine_filesystem STATIC
#
#)
#target_include_directories(engine_filesystem PUBLIC)
## Dependency Libraries:
#target_link_libraries(engine_filesystem PUBLIC)

# 6. Packager
add_library(engine_packager STATIC
        Utilities/Packager/Unpackager.cpp
        Utilities/Packager/Unpackager.hpp
        Utilities/Packager/Packager.cpp
        Utilities/Packager/Packager.hpp
)
target_include_directories(engine_packager PUBLIC Utilities/Packager)
# Dependency Libraries: UtfCpp, Engine.Log
target_link_libraries(engine_packager PUBLIC utfcpp engine_log)

# 7. Windows
add_library(engine_windows STATIC
        Window/RenderWindow.cpp
        Window/RenderWindow.hpp
        Window/Screen.cpp
        Window/Screen.hpp
        Window/Manager/ScreenManager.cpp
        Window/Manager/ScreenManager.hpp
)
# Dependency Libraries: SFML, Engine.Log, Engine.Math
target_link_libraries(engine_windows PUBLIC sfml_all engine_log engine_math)

# 8. Debugger
add_library(engine_debugger STATIC
        Window/Debugger.hpp
        Window/Debugger.cpp
)
target_include_directories(engine_debugger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine_debugger PUBLIC engine_windows imgui_sfml_lib)
if(WIN32)
    target_link_libraries(engine_debugger PUBLIC opengl32 gdi32)
endif()

# 9. Config
add_library(engine_config STATIC
        Config/Components/Entities/NPC.hpp
        Config/Movement/MoveEvent.hpp
)
# Dependency Libraries: Engine.Entity, Engine.Log
target_link_libraries(engine_config PUBLIC engine_entities engine_log)

# 10. Engine Core
add_library(engine_core INTERFACE)
target_link_libraries(engine_core INTERFACE
        engine_math
        engine_entities
        engine_media
        engine_packager
        engine_windows
        engine_log
        engine_lua
        engine_config
)

# 11. Lua Binding
add_library(engine_lua STATIC
        Lua/LuaLoader.hpp
        Lua/LuaLoader.cpp
        Lua/LuaBinding/LuaMusic.cpp
        Lua/LuaBinding/LuaSFX.cpp
        Lua/LuaBinding/LuaEntity.cpp
)
target_include_directories(engine_lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# Dependency Libraries: Engine.All
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
        sfml_all
        ffmpeg
)

# Examples
add_executable(example_media_metadata Example/MediaMetadata.cpp)
target_link_libraries(example_media_metadata PRIVATE taglib)
if(WIN32)
    target_link_libraries(example_media_metadata PRIVATE ws2_32 winmm)
endif()

add_executable(example_audio_playback Example/MusicPlayback.cpp)
target_link_libraries(example_audio_playback PRIVATE engine_media engine_windows sfml_all engine_debugger)
if(WIN32)
    target_link_libraries(example_audio_playback PRIVATE)
endif()

add_executable(example_sfx_playback Example/SFXPlayback.cpp)
target_link_libraries(example_sfx_playback PRIVATE engine_media engine_windows sfml_all engine_debugger)
if(WIN32)
    target_link_libraries(example_sfx_playback PRIVATE)
endif()

add_executable(example_ffmepg_playback Example/FFmpegPlayback.cpp)
target_link_libraries(example_ffmepg_playback PRIVATE ffmpeg engine_media)
if(WIN32)
    target_link_libraries(example_ffmepg_playback PRIVATE)
endif()

add_executable(example_simple_window Example/SimpleWindow.cpp)
target_link_libraries(example_simple_window PRIVATE engine_windows sfml_all)
if(WIN32)
    target_link_libraries(example_simple_window PRIVATE)
endif()

add_executable(example_simple_window_debug Example/SimpleWindowWithDebugger.cpp)
target_link_libraries(example_simple_window_debug PRIVATE engine_windows sfml_all engine_debugger)
if(WIN32)
    target_link_libraries(example_simple_window_debug PRIVATE)
endif()

# ============================================================================
# Tools
# ============================================================================
add_executable(packager_tool
        Utilities/Packager/Package_main.cpp
)
# Dependency Libraries: Engine.Packager
target_link_libraries(packager_tool PRIVATE engine_packager)

# ============================================================================
# Platforms Config
# ============================================================================
# For MINGW
if(MINGW)
    target_link_options(Atom PRIVATE -lstdc++exp -lws2_32)
endif()
if(WIN32)
    target_link_libraries(Atom PRIVATE opengl32 gdi32)
endif()

# ============================================================================
# Post Build Config
# ============================================================================
# For
if(WIN32)
    set(SFML_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/Lib/SFML-3.0.0")
    set(SFML_BIN_DIR "${SFML_ROOT_DIR}/bin")

    if(EXISTS ${SFML_BIN_DIR})
        file(GLOB SFML_DLLS "${SFML_BIN_DIR}/*.dll")
        if(SFML_DLLS)
            add_custom_command(TARGET Atom POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Atom>"
                    COMMAND ${CMAKE_COMMAND} -E copy ${SFML_DLLS} "$<TARGET_FILE_DIR:Atom>"
                    COMMENT "Copying SFML DLLs to output directory"
            )
            message(STATUS "SFML DLLs found: ${SFML_DLLS}")
        else()
            message(WARNING "No DLLs found in ${SFML_BIN_DIR}")
        endif()
    else()
        message(WARNING "SFML bin directory not found: ${SFML_BIN_DIR}")
    endif()
endif()

# FFmpeg
set(FFMPEG_BIN_DIR "${FFMPEG_DIR}/bin")
if(EXISTS ${FFMPEG_BIN_DIR})
    file(GLOB FFMPEG_DLLS "${FFMPEG_BIN_DIR}/*.dll")
    if(FFMPEG_DLLS)
        add_custom_command(TARGET Atom POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${FFMPEG_DLLS} "$<TARGET_FILE_DIR:Atom>"
                COMMENT "Copying FFmpeg DLLs to output directory"
        )
        message(STATUS "FFmpeg DLLs found: ${FFMPEG_DLLS}")
    else()
        message(WARNING "No FFmpeg DLLs found in ${FFMPEG_BIN_DIR}")
    endif()
else()
    message(WARNING "FFmpeg bin directory not found: ${FFMPEG_BIN_DIR}")
endif()

