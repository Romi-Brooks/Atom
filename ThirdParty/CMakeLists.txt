# ============================================================================
# Atom third-party dependencies
# ============================================================================

set(ATOM_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImGUI")
set(ATOM_LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Lua")
set(ATOM_SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL3")
set(ATOM_TAGLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/taglib")
set(ATOM_UTFCPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/utfcpp")
set(ATOM_MINIMP3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/minimp3")
set(ATOM_YOGA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Yoga")

include("${CMAKE_CURRENT_SOURCE_DIR}/TargetOverlay.cmake")

foreach(dependency_file IN ITEMS
        "${ATOM_IMGUI_DIR}/imgui.cpp"
        "${ATOM_LUA_DIR}/lapi.c"
        "${ATOM_SDL3_DIR}/CMakeLists.txt"
        "${ATOM_TAGLIB_DIR}/CMakeLists.txt"
        "${ATOM_UTFCPP_DIR}/CMakeLists.txt"
        "${ATOM_MINIMP3_DIR}/minimp3.h"
        "${ATOM_YOGA_DIR}/yoga/CMakeLists.txt")
    if(NOT EXISTS "${dependency_file}")
        message(FATAL_ERROR
            "Missing third-party dependency content: ${dependency_file}\n"
            "Run: git submodule update --init")
    endif()
endforeach()

# --------------------------------------------------------------------------
# utf8cpp (header-only)
# --------------------------------------------------------------------------
add_library(Atom_3rd_UtfCpp INTERFACE)
target_include_directories(Atom_3rd_UtfCpp INTERFACE "${ATOM_UTFCPP_DIR}/source")

# --------------------------------------------------------------------------
# SDL3
# --------------------------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_UNINSTALL OFF CACHE BOOL "" FORCE)
# The build-tree overlay is intentionally detached from SDL's Git metadata.
# Preserve the pinned submodule's release revision in generated version data.
set(SDL_REVISION "SDL-3.4.12-release-3.4.12" CACHE STRING "" FORCE)

file(SHA256 "${ATOM_SDL3_DIR}/CMakeLists.txt" ATOM_SDL3_CMAKE_HASH)
set(ATOM_SDL3_OVERLAY_DIR "${CMAKE_CURRENT_BINARY_DIR}/TargetOverlays/SDL3")
atom_prepare_target_overlay(
    "${ATOM_SDL3_DIR}"
    "${ATOM_SDL3_OVERLAY_DIR}"
    "${ATOM_SDL3_CMAKE_HASH}:Atom_3rd_SDL3:v1"
)
atom_replace_cmake_identifier("${ATOM_SDL3_OVERLAY_DIR}" "SDL3-static" "Atom_3rd_SDL3")

add_subdirectory(
    "${ATOM_SDL3_OVERLAY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/SDL3"
    EXCLUDE_FROM_ALL
)

# --------------------------------------------------------------------------
# Lua
# --------------------------------------------------------------------------
add_library(Atom_3rd_Lua STATIC
    "${ATOM_LUA_DIR}/lapi.c"
    "${ATOM_LUA_DIR}/lauxlib.c"
    "${ATOM_LUA_DIR}/lbaselib.c"
    "${ATOM_LUA_DIR}/lcode.c"
    "${ATOM_LUA_DIR}/lcorolib.c"
    "${ATOM_LUA_DIR}/lctype.c"
    "${ATOM_LUA_DIR}/ldblib.c"
    "${ATOM_LUA_DIR}/ldebug.c"
    "${ATOM_LUA_DIR}/ldo.c"
    "${ATOM_LUA_DIR}/ldump.c"
    "${ATOM_LUA_DIR}/lfunc.c"
    "${ATOM_LUA_DIR}/lgc.c"
    "${ATOM_LUA_DIR}/linit.c"
    "${ATOM_LUA_DIR}/liolib.c"
    "${ATOM_LUA_DIR}/llex.c"
    "${ATOM_LUA_DIR}/lmathlib.c"
    "${ATOM_LUA_DIR}/lmem.c"
    "${ATOM_LUA_DIR}/loadlib.c"
    "${ATOM_LUA_DIR}/lobject.c"
    "${ATOM_LUA_DIR}/lopcodes.c"
    "${ATOM_LUA_DIR}/loslib.c"
    "${ATOM_LUA_DIR}/lparser.c"
    "${ATOM_LUA_DIR}/lstate.c"
    "${ATOM_LUA_DIR}/lstring.c"
    "${ATOM_LUA_DIR}/lstrlib.c"
    "${ATOM_LUA_DIR}/ltable.c"
    "${ATOM_LUA_DIR}/ltablib.c"
    "${ATOM_LUA_DIR}/ltm.c"
    "${ATOM_LUA_DIR}/lundump.c"
    "${ATOM_LUA_DIR}/lutf8lib.c"
    "${ATOM_LUA_DIR}/lvm.c"
    "${ATOM_LUA_DIR}/lzio.c"
)

target_include_directories(Atom_3rd_Lua PUBLIC "${ATOM_LUA_DIR}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(Atom_3rd_Lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(Atom_3rd_Lua PRIVATE m ${CMAKE_DL_LIBS})
elseif(APPLE)
    target_compile_definitions(Atom_3rd_Lua PRIVATE LUA_USE_MACOSX)
    target_link_libraries(Atom_3rd_Lua PRIVATE ${CMAKE_DL_LIBS})
endif()

# --------------------------------------------------------------------------
# Dear ImGui core and SDL3 backends
# --------------------------------------------------------------------------
add_library(Atom_3rd_ImGui STATIC
    "${ATOM_IMGUI_DIR}/imgui.cpp"
    "${ATOM_IMGUI_DIR}/imgui_draw.cpp"
    "${ATOM_IMGUI_DIR}/imgui_tables.cpp"
    "${ATOM_IMGUI_DIR}/imgui_widgets.cpp"
)

target_include_directories(Atom_3rd_ImGui PUBLIC "${ATOM_IMGUI_DIR}")

add_library(Atom_3rd_ImGui_SDL3 STATIC
    "${ATOM_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${ATOM_IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)

target_include_directories(Atom_3rd_ImGui_SDL3 PUBLIC
    "${ATOM_IMGUI_DIR}"
    "${ATOM_IMGUI_DIR}/backends"
)

target_link_libraries(Atom_3rd_ImGui_SDL3 PUBLIC
    Atom_3rd_ImGui
    Atom_3rd_SDL3
)

# --------------------------------------------------------------------------
# TagLib
# --------------------------------------------------------------------------
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB OFF CACHE BOOL "" FORCE)

# Force TagLib to use Atom's pinned utf8cpp submodule instead of a system copy.
set(utf8cpp_INCLUDE_DIR "${ATOM_UTFCPP_DIR}/source" CACHE PATH "" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_utf8cpp TRUE)

file(SHA256 "${ATOM_TAGLIB_DIR}/CMakeLists.txt" ATOM_TAGLIB_ROOT_CMAKE_HASH)
file(SHA256 "${ATOM_TAGLIB_DIR}/taglib/CMakeLists.txt" ATOM_TAGLIB_LIBRARY_CMAKE_HASH)
set(ATOM_TAGLIB_OVERLAY_DIR "${CMAKE_CURRENT_BINARY_DIR}/TargetOverlays/TagLib")
atom_prepare_target_overlay(
    "${ATOM_TAGLIB_DIR}"
    "${ATOM_TAGLIB_OVERLAY_DIR}"
    "${ATOM_TAGLIB_ROOT_CMAKE_HASH}:${ATOM_TAGLIB_LIBRARY_CMAKE_HASH}:Atom_3rd_TagLib:v1"
)
set(ATOM_TAGLIB_LIBRARY_CMAKE "${ATOM_TAGLIB_OVERLAY_DIR}/taglib/CMakeLists.txt")
atom_replace_in_file("${ATOM_TAGLIB_LIBRARY_CMAKE}" "add_library(tag " "add_library(Atom_3rd_TagLib ")
atom_replace_in_file("${ATOM_TAGLIB_LIBRARY_CMAKE}" "target_include_directories(tag " "target_include_directories(Atom_3rd_TagLib ")
atom_replace_in_file("${ATOM_TAGLIB_LIBRARY_CMAKE}" "target_link_libraries(tag" "target_link_libraries(Atom_3rd_TagLib")
atom_replace_in_file("${ATOM_TAGLIB_LIBRARY_CMAKE}" "set_target_properties(tag " "set_target_properties(Atom_3rd_TagLib ")
atom_replace_in_file("${ATOM_TAGLIB_LIBRARY_CMAKE}" "target_compile_definitions(tag " "target_compile_definitions(Atom_3rd_TagLib ")
atom_replace_in_file("${ATOM_TAGLIB_LIBRARY_CMAKE}" "install(TARGETS tag" "install(TARGETS Atom_3rd_TagLib")
atom_replace_in_file(
    "${ATOM_TAGLIB_OVERLAY_DIR}/CMakeLists.txt"
    "add_custom_target(docs "
    "add_custom_target(Atom_3rd_TagLib_Docs "
)

add_subdirectory(
    "${ATOM_TAGLIB_OVERLAY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/taglib"
    EXCLUDE_FROM_ALL
)

unset(CMAKE_DISABLE_FIND_PACKAGE_utf8cpp)

# TagLib's build-tree target does not expose all public header directories.
# Add them only to the build interface so TagLib's upstream export remains valid.
set(ATOM_TAGLIB_INCLUDE_DIRS
    "${ATOM_TAGLIB_DIR}/taglib"
    "${ATOM_TAGLIB_DIR}/taglib/toolkit"
    "${ATOM_TAGLIB_DIR}/taglib/mpeg"
    "${ATOM_TAGLIB_DIR}/taglib/mpeg/id3v1"
    "${ATOM_TAGLIB_DIR}/taglib/mpeg/id3v2"
    "${ATOM_TAGLIB_DIR}/taglib/mpeg/id3v2/frames"
    "${ATOM_TAGLIB_DIR}/taglib/ogg"
    "${ATOM_TAGLIB_DIR}/taglib/ogg/flac"
    "${ATOM_TAGLIB_DIR}/taglib/ogg/vorbis"
    "${ATOM_TAGLIB_DIR}/taglib/ogg/speex"
    "${ATOM_TAGLIB_DIR}/taglib/ogg/opus"
    "${ATOM_TAGLIB_DIR}/taglib/flac"
    "${ATOM_TAGLIB_DIR}/taglib/mpc"
    "${ATOM_TAGLIB_DIR}/taglib/ape"
    "${ATOM_TAGLIB_DIR}/taglib/wavpack"
    "${ATOM_TAGLIB_DIR}/taglib/mp4"
    "${ATOM_TAGLIB_DIR}/taglib/trueaudio"
    "${ATOM_TAGLIB_DIR}/taglib/riff"
    "${ATOM_TAGLIB_DIR}/taglib/riff/aiff"
    "${ATOM_TAGLIB_DIR}/taglib/riff/wav"
    "${ATOM_TAGLIB_DIR}/taglib/asf"
    "${ATOM_TAGLIB_DIR}/taglib/mod"
    "${ATOM_TAGLIB_DIR}/taglib/s3m"
    "${ATOM_TAGLIB_DIR}/taglib/it"
    "${ATOM_TAGLIB_DIR}/taglib/xm"
    "${ATOM_TAGLIB_DIR}/taglib/dsf"
    "${ATOM_TAGLIB_DIR}/taglib/dsdiff"
    "${ATOM_TAGLIB_DIR}/taglib/shorten"
    "${CMAKE_CURRENT_BINARY_DIR}/taglib"
)

foreach(include_dir IN LISTS ATOM_TAGLIB_INCLUDE_DIRS)
    target_include_directories(
        Atom_3rd_TagLib
        INTERFACE "$<BUILD_INTERFACE:${include_dir}>"
    )
endforeach()

# --------------------------------------------------------------------------
# minimp3 (header-only MP3 decoder, no upstream build system)
# --------------------------------------------------------------------------
add_library(Atom_3rd_Minimp3 INTERFACE)
target_include_directories(Atom_3rd_Minimp3 INTERFACE "${ATOM_MINIMP3_DIR}")

# --------------------------------------------------------------------------
# Yoga (Flexbox layout engine)
# --------------------------------------------------------------------------
# Add only Yoga's core library. The upstream top-level project also enables its
# own test tree and install rules, neither of which belongs in an Atom build.
file(SHA256 "${ATOM_YOGA_DIR}/yoga/CMakeLists.txt" ATOM_YOGA_CMAKE_HASH)
set(ATOM_YOGA_OVERLAY_DIR "${CMAKE_CURRENT_BINARY_DIR}/TargetOverlays/Yoga")
atom_prepare_target_overlay(
    "${ATOM_YOGA_DIR}"
    "${ATOM_YOGA_OVERLAY_DIR}"
    "${ATOM_YOGA_CMAKE_HASH}:Atom_3rd_Yoga:v1"
)
atom_replace_cmake_identifier("${ATOM_YOGA_OVERLAY_DIR}/yoga" "yogacore" "Atom_3rd_Yoga")

add_subdirectory(
    "${ATOM_YOGA_OVERLAY_DIR}/yoga"
    "${CMAKE_CURRENT_BINARY_DIR}/Yoga"
    EXCLUDE_FROM_ALL
)
