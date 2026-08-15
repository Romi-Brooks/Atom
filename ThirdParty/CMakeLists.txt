# ============================================================================
# Atom third-party dependencies
# ============================================================================

set(ATOM_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ImGUI")
set(ATOM_LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Lua")
set(ATOM_SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL3")
set(ATOM_TAGLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/taglib")
set(ATOM_UTFCPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/utfcpp")
set(ATOM_MINIMP3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/minimp3")

foreach(dependency_file IN ITEMS
        "${ATOM_IMGUI_DIR}/imgui.cpp"
        "${ATOM_LUA_DIR}/lapi.c"
        "${ATOM_SDL3_DIR}/CMakeLists.txt"
        "${ATOM_TAGLIB_DIR}/CMakeLists.txt"
        "${ATOM_UTFCPP_DIR}/CMakeLists.txt"
        "${ATOM_MINIMP3_DIR}/minimp3.h")
    if(NOT EXISTS "${dependency_file}")
        message(FATAL_ERROR
            "Missing third-party dependency content: ${dependency_file}\n"
            "Run: git submodule update --init")
    endif()
endforeach()

# --------------------------------------------------------------------------
# utf8cpp (header-only)
# --------------------------------------------------------------------------
add_subdirectory(
    "${ATOM_UTFCPP_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/utfcpp"
    EXCLUDE_FROM_ALL
)

add_library(atom_utfcpp INTERFACE)
target_link_libraries(atom_utfcpp INTERFACE utf8cpp)
add_library(atom::utfcpp ALIAS atom_utfcpp)

# TagLib 2.x also accepts this compatibility target name.
if(NOT TARGET utf8::cpp)
    add_library(utf8::cpp ALIAS utf8cpp)
endif()

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

add_subdirectory(
    "${ATOM_SDL3_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/SDL3"
    EXCLUDE_FROM_ALL
)

# --------------------------------------------------------------------------
# Lua
# --------------------------------------------------------------------------
add_library(atom_lua STATIC
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

target_include_directories(atom_lua PUBLIC "${ATOM_LUA_DIR}")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(atom_lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(atom_lua PRIVATE m ${CMAKE_DL_LIBS})
elseif(APPLE)
    target_compile_definitions(atom_lua PRIVATE LUA_USE_MACOSX)
    target_link_libraries(atom_lua PRIVATE ${CMAKE_DL_LIBS})
endif()

add_library(atom::lua ALIAS atom_lua)

# --------------------------------------------------------------------------
# Dear ImGui core and SDL3 backends
# --------------------------------------------------------------------------
add_library(atom_imgui STATIC
    "${ATOM_IMGUI_DIR}/imgui.cpp"
    "${ATOM_IMGUI_DIR}/imgui_draw.cpp"
    "${ATOM_IMGUI_DIR}/imgui_tables.cpp"
    "${ATOM_IMGUI_DIR}/imgui_widgets.cpp"
)

target_include_directories(atom_imgui PUBLIC "${ATOM_IMGUI_DIR}")
add_library(atom::imgui ALIAS atom_imgui)

add_library(atom_imgui_sdl3 STATIC
    "${ATOM_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${ATOM_IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)

target_include_directories(atom_imgui_sdl3 PUBLIC
    "${ATOM_IMGUI_DIR}"
    "${ATOM_IMGUI_DIR}/backends"
)

target_link_libraries(atom_imgui_sdl3 PUBLIC
    atom::imgui
    SDL3::SDL3
)

add_library(atom::imgui_sdl3 ALIAS atom_imgui_sdl3)

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

add_subdirectory(
    "${ATOM_TAGLIB_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/taglib"
    EXCLUDE_FROM_ALL
)

unset(CMAKE_DISABLE_FIND_PACKAGE_utf8cpp)

# TagLib's build-tree target does not expose all public header directories.
# Keep that upstream detail behind an Atom-owned interface target.
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

add_library(atom_taglib INTERFACE)
target_include_directories(atom_taglib INTERFACE ${ATOM_TAGLIB_INCLUDE_DIRS})
target_link_libraries(atom_taglib INTERFACE tag)
add_library(atom::taglib ALIAS atom_taglib)

# --------------------------------------------------------------------------
# minimp3 (header-only MP3 decoder, no upstream build system)
# --------------------------------------------------------------------------
add_library(atom_minimp3 INTERFACE)
target_include_directories(atom_minimp3 INTERFACE "${ATOM_MINIMP3_DIR}")
add_library(atom::minimp3 ALIAS atom_minimp3)
