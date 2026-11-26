# ============================================================================
# Global Settings
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(Atom VERSION 0.1.0 LANGUAGES C CXX)

# Target naming convention:
#   Atom_*       - engine libraries, backends, applications, and tools
#   Atom_3rd_*   - Atom-owned wrappers around third-party dependencies
#   Example_*    - runnable examples and manual test programs
# Upstream dependency targets remain confined to ThirdParty/CMakeLists.txt.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

option(ATOM_BUILD_SHARED "Build Atom as a shared SDK" OFF)
set(ATOM_ENGINE_LIBRARY_TYPE STATIC)
if(ATOM_BUILD_SHARED)
    set(ATOM_ENGINE_LIBRARY_TYPE OBJECT)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# ============================================================================
# Third-party dependencies
# ============================================================================
add_subdirectory(ThirdParty)

# MSVC otherwise reads UTF-8 sources through the runner's ANSI code page.
# Several engine headers contain Chinese comments, and the exported files are
# BOM-less, so pin the source charset instead of depending on the host locale.
# This sits after ThirdParty so vendored sources keep their own encodings.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # /utf-8: pin the source charset (several headers contain UTF-8 comments
    # and the exported files are BOM-less).
    # /Zc:preprocessor: use the conforming preprocessor; the traditional one
    # mis-expands the variadic channel macros in LogChannelMacros.hpp.
    add_compile_options(/utf-8 /Zc:preprocessor)
endif()

include(Engine/Atom/Presentation/Render/Shader/AtomShaders.cmake)

# ============================================================================
# MinGW runtime linking
# ============================================================================
# Both settings must be in place before the first Atom target is created and
# before add_subdirectory(Test): CMake captures these variables per directory
# scope, so setting them further down this file silently skips every target
# already defined. That is exactly how the test executables ended up dynamically
# linked against libstdc++-6.dll / libgcc_s_seh-1.dll with no copy of either DLL
# produced anywhere, while the executables that did get both flags worked.
if(MINGW)
    # Statically link libgcc and libstdc++ so executables do not require
    # libgcc_s_seh-1.dll / libstdc++-6.dll. Plain "-static" is avoided: this
    # toolchain's static winpthread archive is built for UCRT and references
    # __intrinsic_setjmpex, which the MSVCRT runtime sequence does not provide
    # (undefined reference at link time).
    if(NOT ATOM_BUILD_SHARED)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()

    # libwinpthread stays a DLL, and Windows resolves it next to the loading
    # executable rather than in a parent directory. The build root holds the
    # engine and the tools while AtomTest/ and AtomExample/ hold test and
    # example binaries, so cmake/MinGWRuntime.cmake drops one copy in each.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# Source targets are declared in the root directory scope to retain existing
# binary paths and support cross-module target references.
set(ATOM_ENABLE_DEBUGGER "AUTO" CACHE STRING "Debug overlay: AUTO (follow build type), ON, or OFF")
set_property(CACHE ATOM_ENABLE_DEBUGGER PROPERTY STRINGS AUTO ON OFF)
include(cmake/AtomHelpers.cmake)
option(ATOM_BUILD_TESTS "Build Atom-owned test targets" ON)
include(Engine/Targets.cmake)
include(cmake/Sdk.cmake)

include(cmake/Targets/Examples.cmake)
include(Tools/Targets.cmake)

include(CTest)
# Third-party projects may force the generic BUILD_TESTING cache option off.
# Atom-owned tests use an independent option so that project coverage is not
# silently disabled by a dependency's configuration policy.
if(ATOM_BUILD_TESTS)
    enable_testing()
    # All Atom-owned unit tests live under Test/<Module>/ and register themselves
    # via Test/CMakeLists.txt (CTest names: Atom_<Module>.<Area>).
    add_subdirectory(Test)
endif()

if(NOT ATOM_BUILD_SHARED)
    include(cmake/MinGWRuntime.cmake)
endif()
add_subdirectory(Example/Homeless "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/Example_Homeless")
if(ATOM_BUILD_SHARED)
    include(cmake/SdkRuntime.cmake)
endif()
