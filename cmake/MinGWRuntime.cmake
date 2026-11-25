if(MINGW)
    target_link_options(Atom_Engine PRIVATE -lstdc++exp -lws2_32)

    # The thread runtime stays a DLL. Runtime outputs now split across the build
    # root (engine + tools), AtomTest/ and AtomExample/, and Windows resolves a
    # DLL next to the loading executable rather than in a parent directory, so
    # each directory needs its own copy. Per-target POST_BUILD copies race under
    # Ninja when several executables link in parallel and can fail with
    # ERROR_ACCESS_DENIED, so one custom target produces all copies and every
    # executable depends on it.
    get_filename_component(ATOM_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(ATOM_MINGW_RUNTIME_DLL "${ATOM_MINGW_BIN_DIR}/libwinpthread-1.dll")
    if(EXISTS "${ATOM_MINGW_RUNTIME_DLL}")
        set(atom_mingw_runtime_copies "")
        foreach(atom_runtime_dir IN ITEMS
                "${CMAKE_BINARY_DIR}"
                "${CMAKE_BINARY_DIR}/AtomTest"
                "${CMAKE_BINARY_DIR}/AtomExample")
            set(atom_runtime_copy "${atom_runtime_dir}/libwinpthread-1.dll")
            add_custom_command(
                OUTPUT "${atom_runtime_copy}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${atom_runtime_dir}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${ATOM_MINGW_RUNTIME_DLL}" "${atom_runtime_copy}"
                DEPENDS "${ATOM_MINGW_RUNTIME_DLL}"
                COMMENT "Copying libwinpthread-1.dll to ${atom_runtime_dir}"
                VERBATIM)
            list(APPEND atom_mingw_runtime_copies "${atom_runtime_copy}")
        endforeach()

        add_custom_target(Atom_Mingw_Runtime_DLL DEPENDS ${atom_mingw_runtime_copies})

        foreach(atom_exe IN ITEMS Atom_Engine Example_Audio_Playback
                               Example_Audio_Metadata Example_SFX_Playback
                               Example_Simple_Window Example_Simple_Window_Debug
                               Example_Packaged_Music Example_Log_Channel_Demo
                                Example_MusicCard Example_Lua_Scripting Example_Job_System_Tour
                                Example_Input_Demo Example_UI_Runtime_Demo Example_ECS_Tour Example_Physics_Demo Example_Physics_Benchmark
                               Atom_Tool_Packager)
            if(TARGET ${atom_exe})
                add_dependencies(${atom_exe} Atom_Mingw_Runtime_DLL)
            endif()
        endforeach()
        if(ATOM_BUILD_TESTS)
            get_property(atom_test_targets GLOBAL PROPERTY ATOM_TEST_TARGETS)
            foreach(atom_test IN LISTS atom_test_targets)
                if(TARGET ${atom_test})
                    add_dependencies(${atom_test} Atom_Mingw_Runtime_DLL)
                endif()
            endforeach()
        endif()
    endif()
endif()
