# Executable entry point. The application supplies ApplicationMain.
set(atom_entry_sources Engine/Atom/Platform/ApplicationEntry.cpp Engine/Atom/Platform/ApplicationEntry.hpp)
add_library(Atom_ApplicationEntry OBJECT ${atom_entry_sources})
target_include_directories(Atom_ApplicationEntry PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
if(WIN32)
    add_library(Atom_WindowsDesktopEntry OBJECT ${atom_entry_sources})
    target_include_directories(Atom_WindowsDesktopEntry PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
    target_compile_definitions(Atom_WindowsDesktopEntry PRIVATE "$<${ATOM_WINDOWS_QUIET_RELEASE}:ATOM_WINDOWS_GUI_ENTRY>")
    target_link_libraries(Atom_WindowsDesktopEntry PRIVATE "$<${ATOM_WINDOWS_QUIET_RELEASE}:Atom_3rd_SDL3>")
endif()

# Application resource directory. VFS remains platform-backend neutral.
add_library(Atom_ApplicationPaths STATIC
    Engine/Atom/Platform/ApplicationPaths.hpp
    Engine/Atom/Platform/ApplicationPaths.cpp
)
target_include_directories(Atom_ApplicationPaths PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_ApplicationPaths PUBLIC Atom_Error PRIVATE Atom_Backend_SDL3_Core Atom_UTF8)

add_library(Atom_FileDialogs STATIC Engine/Atom/Platform/FileDialogs.cpp Engine/Atom/Platform/FileDialogs.hpp)
target_include_directories(Atom_FileDialogs PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_FileDialogs PUBLIC Atom_Error PRIVATE Atom_Backend_SDL3_Core)

# Window/render host. It owns native window, backend and frame submission;
# Runtime owns the frame loop, time domains and screen dispatch.
add_library(Atom_Window STATIC
        Engine/Atom/Platform/Window/RenderWindow.hpp
        Engine/Atom/Platform/Window/RenderWindow.cpp
)
target_include_directories(Atom_Window PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Window PUBLIC
    Atom_Backend_Contracts
    Atom_Error
    Atom_InputSystem
    Atom_Log
    Atom_Algorithm)
target_link_libraries(Atom_Window PRIVATE
    Atom_Backend_Extension
    Atom_Backend_Render_Composition)

# Display policy. Owns the two player-facing presentation choices (window
# placement + present mode) and the frame-boundary application of a change.
# It sits above the contracts (it only needs IWindow/IRenderDevice) and below
# Runtime (which ticks it), so neither the window facade nor a debug panel has
# to own the state.
add_library(Atom_Display STATIC
        Engine/Atom/Platform/Display/DisplaySettings.hpp
        Engine/Atom/Platform/Display/DisplaySystem.hpp
        Engine/Atom/Platform/Display/DisplaySystem.cpp
)
target_include_directories(Atom_Display PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Display PUBLIC Atom_Backend_Contracts Atom_Log)
