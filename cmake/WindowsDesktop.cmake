set(ATOM_WINDOWS_QUIET_RELEASE "$<AND:$<CONFIG:Release,MinSizeRel>,$<NOT:$<STREQUAL:${ATOM_ENABLE_DEBUGGER},ON>>>")

function(atom_use_windows_gui_in_release target)
    if(WIN32)
        set_property(TARGET ${target} PROPERTY WIN32_EXECUTABLE "${ATOM_WINDOWS_QUIET_RELEASE}")
        target_compile_definitions(${target} PRIVATE "$<${ATOM_WINDOWS_QUIET_RELEASE}:ATOM_WINDOWS_GUI_ENTRY>")
        target_link_libraries(${target} PRIVATE "$<${ATOM_WINDOWS_QUIET_RELEASE}:Atom_3rd_SDL3>")
    endif()
endfunction()
