# Build a single archive from the engine and its vendored static libraries.
set(archive_targets
    Atom_3rd_SDL3 Atom_3rd_SDL3_mixer Atom_3rd_TagLib Atom_3rd_FreeType
    Atom_3rd_HarfBuzz Atom_3rd_Yoga Atom_3rd_Lua Atom_3rd_ImGui Atom_3rd_ImGui_SDL3)
foreach(module IN LISTS ATOM_SDK_MODULES)
    foreach(target IN LISTS ATOM_${module}_MODULE_TARGETS)
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "STATIC_LIBRARY")
            list(APPEND archive_targets ${target})
        endif()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES archive_targets)

set(atom_sdk_archive_name "${CMAKE_STATIC_LIBRARY_PREFIX}Atom${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(archive_dir "${CMAKE_CURRENT_BINARY_DIR}/sdk-archive/$<CONFIG>")
set(archive "${archive_dir}/${atom_sdk_archive_name}")
set(archive_input "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/AtomSdkArchive-$<CONFIG>.cmake")
set(merge_script "${CMAKE_CURRENT_LIST_DIR}/MergeSdkArchives.cmake")
set(archive_files "")
foreach(target IN LISTS archive_targets)
    list(APPEND archive_files "$<TARGET_FILE:${target}>")
endforeach()
file(GENERATE OUTPUT "${archive_input}" CONTENT "set(archives [==[${archive_files}]==])\n")
add_custom_command(OUTPUT "${archive}"
    COMMAND "${CMAKE_COMMAND}" "-Darchiver=${CMAKE_AR}" "-Dmsvc=${MSVC}"
        "-Darchive=${archive}" "-Dinputs=${archive_input}" -P "${merge_script}"
    DEPENDS ${archive_targets} "${archive_input}" "${merge_script}"
    COMMENT "Assembling the Atom static SDK archive" VERBATIM)
add_custom_target(AtomSdkArchive DEPENDS "${archive}")
add_dependencies(AtomSdkArchive Atom_Entry Atom_Shaders)
if(TARGET Atom_Entry_Windowed)
    add_dependencies(AtomSdkArchive Atom_Entry_Windowed)
endif()
install(FILES "${archive}" DESTINATION "${CMAKE_INSTALL_LIBDIR}")

# Preserve the platform libraries and link options used by the pinned SDL
# build. Header-only targets are already covered by the SDK's own includes.
get_target_property(atom_static_system_libraries Atom_3rd_SDL3 LINK_LIBRARIES)
if(NOT atom_static_system_libraries)
    set(atom_static_system_libraries "")
endif()
list(FILTER atom_static_system_libraries EXCLUDE REGEX "SDL3::Headers|SDL3_Headers$")
get_target_property(atom_static_link_options Atom_3rd_SDL3 INTERFACE_LINK_OPTIONS)
if(NOT atom_static_link_options)
    set(atom_static_link_options "")
endif()
if(WIN32)
    list(APPEND atom_static_system_libraries opengl32 gdi32)
elseif(UNIX)
    list(APPEND atom_static_system_libraries Threads::Threads ${CMAKE_DL_LIBS})
endif()
list(REMOVE_DUPLICATES atom_static_system_libraries)

configure_file("${CMAKE_CURRENT_LIST_DIR}/AtomStaticTargets.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AtomStaticTargets.cmake" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AtomStaticTargets.cmake"
    DESTINATION "${atom_sdk_cmake_dir}")
