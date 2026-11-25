# Input value types (header-only, no platform/backend dependency).
add_library(Atom_Input INTERFACE)
target_sources(Atom_Input INTERFACE
    Engine/Atom/Input/Key.hpp
    Engine/Atom/Input/ActionMap.hpp
)
target_include_directories(Atom_Input INTERFACE
    "${PROJECT_SOURCE_DIR}/Engine"
)

# Input system — per-frame keyboard/mouse state folded from the window
#     event stream. Consumes window::IEvent, so it sits above the contracts.
add_library(Atom_InputSystem STATIC
    Engine/Atom/Input/InputSystem.hpp
    Engine/Atom/Input/InputSystem.cpp
    # Gameplay-facing action queries (axes, edges, pause gate). Header-only: a thin
    # typed view over InputSystem, templated on the caller's own action enum.
    Engine/Atom/Input/ActionState.hpp
)
target_include_directories(Atom_InputSystem PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
# Atom_Log is PUBLIC because a consumer that logs its own InputSystem reads
# (Engine/Atom/Core/Log/AtomLogChannels.hpp) resolves the channel symbols in its own objects.
target_link_libraries(Atom_InputSystem PUBLIC Atom_Backend_Contracts Atom_Log)
