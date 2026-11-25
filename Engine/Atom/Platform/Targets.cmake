# Executable entry point. The application supplies ApplicationMain.
add_library(Atom_Entry STATIC Engine/Atom/Platform/Entry/ConsoleMain.cpp)
add_library(Atom::Entry ALIAS Atom_Entry)
target_include_directories(Atom_Entry PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Engine>")
set_target_properties(Atom_Entry PROPERTIES OUTPUT_NAME AtomEntry EXPORT_NAME Entry)
if(WIN32)
    add_library(Atom_Entry_Windowed STATIC Engine/Atom/Platform/Entry/WindowsMain.cpp)
    add_library(Atom::EntryWindowed ALIAS Atom_Entry_Windowed)
    target_include_directories(Atom_Entry_Windowed PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Engine>")
    target_link_libraries(Atom_Entry_Windowed PRIVATE shell32)
    if(MINGW)
        # MinGW's CRT references WinMain after the linker has scanned user archives.
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            target_link_options(Atom_Entry_Windowed INTERFACE "LINKER:--undefined=WinMain")
        else()
            target_link_options(Atom_Entry_Windowed INTERFACE "LINKER:--undefined=_WinMain@16")
        endif()
    endif()
    set_target_properties(Atom_Entry_Windowed PROPERTIES OUTPUT_NAME AtomEntryWindowed EXPORT_NAME EntryWindowed)
endif()

# Application resource directory. VFS remains platform-backend neutral.
add_library(Atom_ApplicationPaths ${ATOM_ENGINE_LIBRARY_TYPE}
    Engine/Atom/Platform/ApplicationPaths.hpp
    Engine/Atom/Platform/ApplicationPaths.cpp
)
target_include_directories(Atom_ApplicationPaths PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_ApplicationPaths PUBLIC Atom_Error PRIVATE Atom_Backend_SDL3_Core Atom_UTF8)

add_library(Atom_FileDialogs ${ATOM_ENGINE_LIBRARY_TYPE} Engine/Atom/Platform/FileDialogs.cpp Engine/Atom/Platform/FileDialogs.hpp)
target_include_directories(Atom_FileDialogs PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_FileDialogs PUBLIC Atom_Error PRIVATE Atom_Backend_SDL3_Core)

# Window/render host. It owns native window, backend and frame submission;
# Runtime owns the frame loop, time domains and screen dispatch.
add_library(Atom_Window ${ATOM_ENGINE_LIBRARY_TYPE}
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
add_library(Atom_Display ${ATOM_ENGINE_LIBRARY_TYPE}
        Engine/Atom/Platform/Display/DisplaySettings.hpp
        Engine/Atom/Platform/Display/DisplaySystem.hpp
        Engine/Atom/Platform/Display/DisplaySystem.cpp
)
target_include_directories(Atom_Display PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Display PUBLIC Atom_Backend_Contracts Atom_Log)
