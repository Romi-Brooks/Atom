# ============================================================================
# Global Settings
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(Atom LANGUAGES C CXX)

# Target naming convention:
#   Atom_*       - engine libraries, backends, applications, and tools
#   Atom_3rd_*   - Atom-owned wrappers around third-party dependencies
#   Example_*    - runnable examples and manual test programs
# Upstream dependency targets remain confined to ThirdParty/CMakeLists.txt.

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# ============================================================================
# Third-party dependencies
# ============================================================================
add_subdirectory(ThirdParty)

include(Engine/Presentation/Render/Shader/AtomShaders.cmake)

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

# Source targets are declared in the root directory scope to retain existing
# binary paths and support cross-module target references.
set(ATOM_ENABLE_DEBUGGER "AUTO" CACHE STRING "Debug overlay: AUTO (follow build type), ON, or OFF")
set_property(CACHE ATOM_ENABLE_DEBUGGER PROPERTY STRINGS AUTO ON OFF)
include(cmake/ApplicationEntry.cmake)
include(Engine/Targets.cmake)

# Main Exec
add_executable(Atom_Engine Engine/Atom.cpp)
set_target_properties(Atom_Engine PROPERTIES OUTPUT_NAME Atom)

target_link_libraries(Atom_Engine PRIVATE
        Atom_All
)
if(WIN32)
    target_link_libraries(Atom_Engine PRIVATE opengl32 gdi32)
endif()

include(cmake/Targets/Examples.cmake)
include(Tools/Targets.cmake)

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

include(cmake/MinGWRuntime.cmake)

add_subdirectory(Example/Homeless)
