# Engine targets grouped by their SDK DLL owner.
set(ATOM_CORE_MODULE_TARGETS
    Atom_Algorithm
    Atom_Assets
    Atom_Color
    Atom_Concurrency
    Atom_ECS
    Atom_Error
    Atom_FS
    Atom_Input
    Atom_InputSystem
    Atom_Backend_Contracts
    Atom_Json
    Atom_Log
    Atom_Math
    Atom_Packager
    Atom_Random
    Atom_Sprite
    Atom_Threading
    Atom_Time
    Atom_UTF8
)

set(ATOM_WORLD_MODULE_TARGETS
    Atom_Physics
    Atom_PhysicsIntegration
    Atom_Scene
)

set(ATOM_BACKENDS_MODULE_TARGETS
    Atom_Audio_Codecs
    Atom_Backend_Audio
    Atom_Backend_Composition
    Atom_Backend_Extension
    Atom_Backend_Render_Composition
    Atom_Backend_Render_Runtime
    Atom_Backend_Runtime
    Atom_Backend_SDL_GPU
    Atom_Backend_SDL3_Audio
    Atom_Backend_SDL3_Core
    Atom_Backend_SDL3_Mixer_Audio
    Atom_Backend_SDL3_Window
)

set(ATOM_PLATFORM_MODULE_TARGETS
    Atom_ApplicationPaths
    Atom_Display
    Atom_FileDialogs
    Atom_Window
)

set(ATOM_RUNTIME_MODULE_TARGETS
    Atom_Runtime
)

set(ATOM_GRAPHICS_MODULE_TARGETS
    Atom_Layout
    Atom_Render_Core
    Atom_Render_Pipeline
    Atom_Render_Resources
    Atom_Renderer2D
    Atom_Scene2D
    Atom_Text
    Atom_UI
    Atom_World2D
)

set(ATOM_MEDIA_AUDIO_MODULE_TARGETS
    Atom_Media
)

set(ATOM_MEDIA_IMAGE_MODULE_TARGETS
    Atom_Image
)

set(ATOM_SCRIPTING_MODULE_TARGETS
    Atom_Lua
)

set(ATOM_DEBUGGER_MODULE_TARGETS
    Atom_Backend_SDLGPU_Debug
    Atom_Debugger
)

foreach(module IN ITEMS CORE WORLD BACKENDS PLATFORM RUNTIME GRAPHICS MEDIA_AUDIO MEDIA_IMAGE SCRIPTING DEBUGGER)
    foreach(target IN LISTS ATOM_${module}_MODULE_TARGETS)
        get_target_property(type ${target} TYPE)
        if(NOT type STREQUAL "INTERFACE_LIBRARY")
            target_compile_definitions(${target} PRIVATE ATOM_${module}_EXPORTS)
        endif()
    endforeach()
endforeach()
