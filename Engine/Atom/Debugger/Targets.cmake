# Debugger
# Debug overlay build switch. AUTO (default) follows the build type:
# enabled in Debug, stripped in Release/MinSizeRel. Force either way with
# -DATOM_ENABLE_DEBUGGER=ON (e.g. to ship a Release build that still shows
# the debug overlay, or a future profiler built on the same panels) or
# =OFF. The resolved value is propagated as an explicit 0/1 macro so no
# translation unit relies on the header-side NDEBUG heuristic.
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
        Engine/Atom/Debugger/DebugPanel.hpp
        Engine/Atom/Debugger/DebugPanel.cpp
        Engine/Atom/Debugger/Panels/LogDebugger.hpp
        Engine/Atom/Debugger/Panels/LogDebugger.cpp
        Engine/Atom/Debugger/FontLoader.hpp
        Engine/Atom/Debugger/FontLoader.cpp
        Engine/Atom/Debugger/PanelLayout.hpp
        Engine/Atom/Debugger/PanelLayout.cpp
        Engine/Atom/Debugger/Overlay.hpp
        Engine/Atom/Debugger/OverlayManager.hpp
        Engine/Atom/Debugger/OverlayManager.cpp
        Engine/Atom/Debugger/Panels/JobDebugger.hpp
        Engine/Atom/Debugger/Panels/JobDebugger.cpp
        Engine/Atom/Debugger/Panels/ThreadDebugger.hpp
        Engine/Atom/Debugger/Panels/ThreadDebugger.cpp
        Engine/Atom/Debugger/Panels/FpsDebugger.hpp
        Engine/Atom/Debugger/Panels/FpsDebugger.cpp
        Engine/Atom/Debugger/Panels/RenderDebugger.hpp
        Engine/Atom/Debugger/Panels/RenderDebugger.cpp
        Engine/Atom/Debugger/Panels/DisplayDebugger.hpp
        Engine/Atom/Debugger/Panels/DisplayDebugger.cpp
)
target_include_directories(Atom_Debugger PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_compile_definitions(Atom_Debugger PUBLIC ATOM_ENABLE_DEBUGGER=$<BOOL:${_atom_debugger_enabled}>)
target_compile_definitions(Atom_Window PUBLIC ATOM_ENABLE_DEBUGGER=$<BOOL:${_atom_debugger_enabled}>)
target_link_libraries(Atom_Debugger PUBLIC Atom_3rd_ImGui_SDL3 Atom_Concurrency)

# Runtime is a .cpp-only dependency here: no public Debugger header includes Runtime/, so
# keeping it PRIVATE stops every overlay consumer from inheriting the Runtime dependency tree as well.
target_link_libraries(Atom_Debugger PRIVATE Atom_Runtime Atom_Backend_SDLGPU_Debug)
if(WIN32)
    target_link_libraries(Atom_Debugger PUBLIC opengl32 gdi32)
endif()
