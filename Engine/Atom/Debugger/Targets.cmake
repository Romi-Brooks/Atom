# Debugger
# The shared SDK always enables its debugger. In static builds, AUTO follows
# the build type and ATOM_ENABLE_DEBUGGER can override it. Propagate the
# resolved value as 0/1 so engine and consumer code agree.
set(_atom_debugger_enabled OFF)
if(ATOM_BUILD_SHARED OR ATOM_ENABLE_DEBUGGER STREQUAL "ON")
    set(_atom_debugger_enabled ON)
elseif(ATOM_ENABLE_DEBUGGER STREQUAL "AUTO")
    # Single-config Ninja builds set CMAKE_BUILD_TYPE; the header fallback in
    # DebuggerConfig.hpp still covers non-CMake or multi-config consumers.
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        set(_atom_debugger_enabled ON)
    endif()
endif()

add_library(Atom_Debugger ${ATOM_ENGINE_LIBRARY_TYPE}
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
