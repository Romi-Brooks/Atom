# ============================================================================
# Third-party target-name overlay helpers
# ============================================================================
# CMake does not allow an existing logical target to be renamed. Some upstream
# projects also do not expose a target-name option. Keep pinned submodules
# pristine by copying those projects into the build tree once and changing only
# their CMake target identifiers there.

function(atom_prepare_target_overlay source_dir overlay_dir signature)
    set(stamp_file "${overlay_dir}/.atom-target-overlay")
    set(current_signature "")
    if(EXISTS "${stamp_file}")
        file(READ "${stamp_file}" current_signature)
    endif()

    if(NOT current_signature STREQUAL signature)
        if(NOT EXISTS "${source_dir}")
            message(FATAL_ERROR "[Atom.ThirdParty] Overlay source does not exist: ${source_dir}")
        endif()
        file(REMOVE_RECURSE "${overlay_dir}")
        file(MAKE_DIRECTORY "${overlay_dir}")
        file(COPY "${source_dir}/" DESTINATION "${overlay_dir}")
        file(WRITE "${stamp_file}" "${signature}")
        message(STATUS "[Atom.ThirdParty] Prepared target overlay: ${overlay_dir}")
    else()
        message(DEBUG "[Atom.ThirdParty] Reusing target overlay: ${overlay_dir}")
    endif()
endfunction()

function(atom_replace_in_file file_path old_text new_text)
    file(READ "${file_path}" contents)
    string(REPLACE "${old_text}" "${new_text}" updated_contents "${contents}")
    if(NOT updated_contents STREQUAL contents)
        file(WRITE "${file_path}" "${updated_contents}")
        message(DEBUG "[Atom.ThirdParty] Replaced '${old_text}' with '${new_text}' in ${file_path}")
    endif()
endfunction()

function(atom_replace_cmake_identifier root_dir old_identifier new_identifier)
    file(GLOB_RECURSE cmake_files LIST_DIRECTORIES FALSE
        "${root_dir}/CMakeLists.txt"
        "${root_dir}/*.cmake")
    foreach(cmake_file IN LISTS cmake_files)
        atom_replace_in_file("${cmake_file}" "${old_identifier}" "${new_identifier}")
    endforeach()
endfunction()
