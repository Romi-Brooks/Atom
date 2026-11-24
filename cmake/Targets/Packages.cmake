# Convenience package
# Atom_All is intentionally a convenience package for applications and
# legacy samples. Engine modules and new samples must link the smallest
# explicit set of domain targets they consume.
add_library(Atom_All INTERFACE)
target_link_libraries(Atom_All INTERFACE
        Atom_Algorithm
        Atom_Random
        # Time is consumed directly by upper layers through
        # TimeSystem::GetInstance(), not only transitively via Atom_Window.
        Atom_Time
        Atom_Concurrency
        Atom_Layout
        Atom_Media
        Atom_Packager
        Atom_Window
        Atom_Runtime
        Atom_ApplicationPaths
        Atom_UI
        Atom_ECS
        Atom_Physics
        Atom_Log
        Atom_Lua
        Atom_UTF8
        Atom_FS
        Atom_Assets
        Atom_Renderer2D
        Atom_Render_Resources
        Atom_Image
)

# Compatibility only. Keep this target while external consumers migrate;
# do not add new dependencies or new users of Atom_Core.
add_library(Atom_Core INTERFACE)
target_link_libraries(Atom_Core INTERFACE Atom_All)
