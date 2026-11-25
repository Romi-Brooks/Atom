add_library(Atom_Tool_PackagerOperations STATIC
    Tools/Packager/PackagerOperations.cpp
    Tools/Packager/Utf8Arguments.cpp)
target_link_libraries(Atom_Tool_PackagerOperations PUBLIC Atom_FS PRIVATE Atom_Packager Atom_Log Atom_UTF8)
if(WIN32)
    target_link_libraries(Atom_Tool_PackagerOperations PRIVATE shell32)
endif()

add_executable(Atom_Tool_Packager
        Tools/Packager/Package.cpp
)
set_target_properties(Atom_Tool_Packager PROPERTIES OUTPUT_NAME packager_tool)
target_link_libraries(Atom_Tool_Packager PRIVATE Atom_Tool_PackagerOperations Atom_Log Atom_UTF8)

if(WIN32)
    # Preserve shell waiting, exit codes and redirection for CLI/TUI commands.
    # Windows 11 24H2+ skips console creation for desktop launches with this manifest.
    set(ATOM_WINDOWS_QUIET_RELEASE "$<AND:$<CONFIG:Release,MinSizeRel>,$<NOT:$<STREQUAL:${ATOM_ENABLE_DEBUGGER},ON>>>")
    set(packager_manifest "${CMAKE_CURRENT_LIST_DIR}/Packager/Packager.manifest")
    if(MSVC)
        target_sources(Atom_Tool_Packager PRIVATE "$<${ATOM_WINDOWS_QUIET_RELEASE}:${packager_manifest}>")
    else()
        enable_language(RC)
        target_sources(Atom_Tool_Packager PRIVATE "$<${ATOM_WINDOWS_QUIET_RELEASE}:${CMAKE_CURRENT_LIST_DIR}/Packager/Packager.rc>")
        set_property(SOURCE Tools/Packager/Packager.rc APPEND PROPERTY OBJECT_DEPENDS "${packager_manifest}")
    endif()
endif()

# Desktop packager driven by Atom's runtime and renderer.
add_library(Atom_Tool_PackagerDesktop STATIC
    Tools/Packager/PackagerGui.cpp
    Tools/Packager/PackagerPreview.cpp)
target_link_libraries(Atom_Tool_PackagerDesktop PRIVATE
    Atom_Runtime Atom_UI Atom_ApplicationPaths Atom_FileDialogs Atom_Concurrency Atom_Log Atom_UTF8
    Atom_Tool_PackagerOperations Atom_Image Atom_Media Atom_Json)
target_compile_definitions(Atom_Tool_PackagerDesktop PRIVATE ATOM_SHADER_OUTPUT_DIR="${ATOM_SHADER_OUTPUT_DIR}")
add_dependencies(Atom_Tool_PackagerDesktop Atom_Shaders)

target_link_libraries(Atom_Tool_Packager PRIVATE Atom_Tool_PackagerDesktop)
