# Runtime. This is the sole game frame-loop owner; Window intentionally
# remains usable by tools that only need a platform/render host.
add_library(Atom_Runtime STATIC
        Engine/Atom/Runtime/Application.hpp
        Engine/Atom/Runtime/Application.cpp
        Engine/Atom/Runtime/EntityCommandBuffer.hpp
        Engine/Atom/Runtime/EntityCommandBuffer.cpp
        Engine/Atom/Runtime/GameEventQueue.hpp
        Engine/Atom/Runtime/GameEventQueue.cpp
        Engine/Atom/Runtime/GameScreen.hpp
        Engine/Atom/Runtime/GameScreen.cpp
        Engine/Atom/Runtime/GameWorld.hpp
        Engine/Atom/Runtime/GameWorld.cpp
        Engine/Atom/Runtime/Screen.hpp
        Engine/Atom/Runtime/ScreenManager.hpp
        Engine/Atom/Runtime/ScreenManager.cpp
        Engine/Atom/Runtime/System.hpp
        Engine/Atom/Runtime/SystemScheduler.hpp
        Engine/Atom/Runtime/SystemScheduler.cpp
)
target_include_directories(Atom_Runtime PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Runtime PUBLIC Atom_Window Atom_Display Atom_Time Atom_Log Atom_ECS)
