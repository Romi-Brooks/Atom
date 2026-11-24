if(MINGW)
    target_link_options(Atom_Engine PRIVATE -lstdc++exp -lws2_32)

    # The thread runtime stays a DLL. The MinGW configuration in the root CMakeLists.txt
    # places every Atom executable, tests included, in CMAKE_BINARY_DIR, so copy
    # the DLL once into that directory instead of attaching a POST_BUILD copy to
    # every executable. Per-target POST_BUILD copies race under Ninja when
    # several executables link in parallel and can fail with ERROR_ACCESS_DENIED.
    get_filename_component(ATOM_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(ATOM_MINGW_RUNTIME_DLL "${ATOM_MINGW_BIN_DIR}/libwinpthread-1.dll")
    if(EXISTS "${ATOM_MINGW_RUNTIME_DLL}")
        set(ATOM_MINGW_RUNTIME_DLL_OUTPUT
            "${CMAKE_BINARY_DIR}/libwinpthread-1.dll")
        add_custom_command(
            OUTPUT "${ATOM_MINGW_RUNTIME_DLL_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ATOM_MINGW_RUNTIME_DLL}"
                    "${ATOM_MINGW_RUNTIME_DLL_OUTPUT}"
            DEPENDS "${ATOM_MINGW_RUNTIME_DLL}"
            COMMENT "Copying libwinpthread-1.dll to the runtime output directory"
            VERBATIM)
        add_custom_target(Atom_Mingw_Runtime_DLL
            DEPENDS "${ATOM_MINGW_RUNTIME_DLL_OUTPUT}")
        foreach(atom_exe IN ITEMS Atom_Engine Example_Audio_Playback
                               Example_Audio_Metadata Example_SFX_Playback
                               Example_Simple_Window Example_Simple_Window_Debug
                               Example_Packaged_Music Example_Log_Channel_Demo
                                Example_MusicCard Example_Lua_Scripting Example_Job_System_Tour
                                Example_Input_Demo Example_UI_Runtime_Demo Example_ECS_Tour Example_Physics_Demo Example_Physics_Benchmark
                               Atom_Tool_Packager)
            add_dependencies(${atom_exe} Atom_Mingw_Runtime_DLL)
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
