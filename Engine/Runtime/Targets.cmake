# Runtime. This is the sole game frame-loop owner; Window intentionally
# remains usable by tools that only need a platform/render host.
add_library(Atom_Runtime STATIC
        Engine/Runtime/Application.hpp
        Engine/Runtime/Application.cpp
        Engine/Runtime/EntityCommandBuffer.hpp
        Engine/Runtime/EntityCommandBuffer.cpp
        Engine/Runtime/GameEventQueue.hpp
        Engine/Runtime/GameEventQueue.cpp
        Engine/Runtime/GameScreen.hpp
        Engine/Runtime/GameScreen.cpp
        Engine/Runtime/GameWorld.hpp
        Engine/Runtime/GameWorld.cpp
        Engine/Runtime/Screen.hpp
        Engine/Runtime/ScreenManager.hpp
        Engine/Runtime/ScreenManager.cpp
        Engine/Runtime/System.hpp
        Engine/Runtime/SystemScheduler.hpp
        Engine/Runtime/SystemScheduler.cpp
)
target_include_directories(Atom_Runtime PUBLIC "${PROJECT_SOURCE_DIR}/Engine")
target_link_libraries(Atom_Runtime PUBLIC Atom_Window Atom_Display Atom_Time Atom_Log Atom_ECS)
