# Examples
add_executable(Example_Audio_Playback Example/Media/AudioPlayback.cpp)
target_link_libraries(Example_Audio_Playback PRIVATE
        Atom_Media Atom_Runtime Atom_Debugger)

add_executable(Example_SFX_Playback Example/Media/SFXPlayback.cpp)
target_link_libraries(Example_SFX_Playback PRIVATE
        Atom_Media Atom_Runtime Atom_Debugger)

add_executable(Example_Simple_Window Example/Window/SimpleWindow.cpp)
target_link_libraries(Example_Simple_Window PRIVATE Atom_Runtime)

add_executable(Example_Simple_Window_Debug Example/Window/SimpleWindowWithDebugger.cpp)
target_link_libraries(Example_Simple_Window_Debug PRIVATE Atom_Runtime Atom_Debugger)

# Audio metadata example (uses the engine's AudioMetadataReader)
add_executable(Example_Audio_Metadata Example/Media/AudioMetadata.cpp)
target_link_libraries(Example_Audio_Metadata PRIVATE Atom_Media Atom_Concurrency)

# Job system tutorial — a self-contained walkthrough of the scheduler API.
add_executable(Example_Job_System_Tour Example/Concurrency/JobSystemTour.cpp)
target_link_libraries(Example_Job_System_Tour PRIVATE Atom_Concurrency)

# Resource-pack playback example
add_executable(Example_Packaged_Music Example/Media/PackagedMusicPlayback.cpp)
target_link_libraries(Example_Packaged_Music PRIVATE
        Atom_Media Atom_Runtime Atom_Debugger Atom_Packager)

# Log channel
add_executable(Example_Log_Channel_Demo Example/Log/LogChannelDemo.cpp Example/Log/GameChannels.hpp)
target_link_libraries(Example_Log_Channel_Demo PRIVATE Atom_Log)

# Lua scripting example (loads an external script and plays a track via Lua)
add_executable(Example_Lua_Scripting Example/Lua/LuaScripting.cpp)
target_link_libraries(Example_Lua_Scripting PRIVATE
        Atom_Lua Atom_Media Atom_Log Atom_Runtime Atom_Debugger)

# MusicCard example — Yoga music HUD card migrated onto Renderer2D (phase C):
# cover art decode via Atom_Image, production drawing/text via Renderer2D,
# and an optional ImGui overlay for debug controls.
add_executable(Example_MusicCard
    Example/MusicCard/MusicCard.cpp
    Example/MusicCard/MusicCardScreen.cpp
    Example/MusicCard/MusicCardTypes.hpp
    Example/MusicCard/MusicCardScreen.hpp
    Example/MusicCard/MusicCardDebugger.hpp
)
target_link_libraries(Example_MusicCard PRIVATE
    Atom_Media Atom_Runtime Atom_Debugger Atom_Layout Atom_Renderer2D Atom_Render_Resources Atom_Image)
target_compile_definitions(Example_MusicCard PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_MusicCard Atom_Shaders)
# Resource telemetry reads the process working set via the Win32 PSAPI.
if(WIN32)
    target_link_libraries(Example_MusicCard PRIVATE psapi)
endif()

# CORE-002 acceptance example — a box moved by the per-frame InputSystem, plus
# an overlay that prints the raw state. Proves the main loop feeds the input
# state machine, so the screen never touches SDL or Window events itself.
add_executable(Example_Input_Demo Example/Input/InputDemo.cpp)
target_link_libraries(Example_Input_Demo PRIVATE Atom_Runtime Atom_Debugger Atom_Renderer2D)
target_compile_definitions(Example_Input_Demo PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_Input_Demo Atom_Shaders)

# Retained game-UI acceptance example. It uses Runtime's ScreenManager for a
# menu/settings/game/pause flow; UI only owns widgets inside each page.
add_executable(Example_UI_Runtime_Demo Example/UI/UiRuntimeDemo.cpp)
target_link_libraries(Example_UI_Runtime_Demo PRIVATE Atom_Runtime Atom_UI)
target_compile_definitions(Example_UI_Runtime_Demo PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_UI_Runtime_Demo Atom_Shaders)

# Physics demo — interactive Renderer2D visualization of the Internal physics
# backend: gravity, stacking, a pendulum, a revolute chain, and click-to-spawn.
add_executable(Example_Physics_Demo Example/Physics/PhysicsDemo.cpp)
target_link_libraries(Example_Physics_Demo PRIVATE
    Atom_Physics Atom_Runtime Atom_Renderer2D Atom_Debugger)
target_compile_definitions(Example_Physics_Demo PRIVATE
    ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Example_Physics_Demo Atom_Shaders)

# Headless baseline for the physics JobSystem profiling gate. It intentionally
# has no renderer/window dependency so measurements isolate Step().
add_executable(Example_Physics_Benchmark Example/Physics/PhysicsBenchmark.cpp)
target_link_libraries(Example_Physics_Benchmark PRIVATE Atom_Physics Atom_Concurrency)

# ECS tour — a headless console walk through the entity/component store, ending in
# a real integration loop. Console rather than windowed on purpose: the ECS is pure
# storage, so the module can be exercised without a GPU, and this example doubles as
# the template for the first physics system.
add_executable(Example_ECS_Tour Example/ECS/EntityWorldTour.cpp)
target_link_libraries(Example_ECS_Tour PRIVATE Atom_ECS)

# Examples are runnable programs, not the engine: keep their binaries in
# AtomExample/ so the build root only holds the engine and the tools.
foreach(window_example IN ITEMS
        Example_Audio_Playback
        Example_SFX_Playback
        Example_Simple_Window
        Example_Simple_Window_Debug
        Example_Packaged_Music
        Example_Lua_Scripting
        Example_MusicCard
        Example_Input_Demo
        Example_UI_Runtime_Demo
        Example_Physics_Demo)
    set_target_properties(${window_example} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/AtomExample")
    atom_use_application_entry(${window_example} WINDOWED)
endforeach()

foreach(console_example IN ITEMS
        Example_Audio_Metadata
        Example_Job_System_Tour
        Example_Log_Channel_Demo
        Example_Physics_Benchmark
        Example_ECS_Tour)
    set_target_properties(${console_example} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/AtomExample")
    atom_use_application_entry(${console_example} CONSOLE)
endforeach()
