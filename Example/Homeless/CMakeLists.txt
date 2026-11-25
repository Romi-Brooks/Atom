add_executable(Example_Homeless
    Source/Homeless.cpp
    Source/Content/ContentMount.cpp
    Source/Content/LocalPlaylist.cpp
    Source/Debug/PromenadeDebugger.cpp
    Source/Scene/PromenadeScreen.cpp
    Source/Scene/PromenadeAssets.cpp
    Source/Scene/PromenadeAreas.cpp
    Source/Scene/PromenadeWalker.cpp
    Source/Scene/PromenadeWeather.cpp
    Source/Scene/Pedestrians.cpp
    Source/Scene/SceneAudio.cpp
    Source/Scene/Traffic.cpp
    Source/UI/GameOverlay.cpp
    Source/UI/GameOverlayMenu.cpp
    Source/UI/GameOverlayPlayer.cpp
)
target_compile_features(Example_Homeless PRIVATE cxx_std_23)
target_include_directories(Example_Homeless PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_link_libraries(Example_Homeless PRIVATE
    Atom_Runtime Atom_World2D Atom_Render_Resources Atom_Media Atom_Debugger Atom_Concurrency Atom_Random Atom_Image
    Atom_ApplicationPaths Atom_FS Atom_Log)
set_target_properties(Example_Homeless PROPERTIES
    OUTPUT_NAME Homeless
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Game")

atom_use_application_entry(Example_Homeless WINDOWED)

get_property(shader_directory TARGET Atom_Shaders PROPERTY ATOM_SHADER_DIRECTORY)
get_property(shader_files TARGET Atom_Shaders PROPERTY ATOM_SHADER_FILES)
set(game_directory "$<TARGET_FILE_DIR:Example_Homeless>")
file(GLOB_RECURSE content_files CONFIGURE_DEPENDS LIST_DIRECTORIES false "${CMAKE_CURRENT_SOURCE_DIR}/Content/*")

set(staged_stamp "${CMAKE_CURRENT_BINARY_DIR}/Example_Homeless_content-$<CONFIG>.stamp")
set(stage_commands
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Content" "${game_directory}/Content"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${shader_directory}" "${game_directory}/Shaders"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${game_directory}/Music")
set(stage_depends Atom_Shaders ${shader_files} ${content_files})
if(MINGW AND TARGET Atom_Mingw_Runtime_DLL)
    # MinGW links libstdc++/libgcc statically, but the threading runtime stays a
    # DLL; ship it next to the executable.
    list(APPEND stage_commands COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/libwinpthread-1.dll" "${game_directory}")
    list(APPEND stage_depends Atom_Mingw_Runtime_DLL "${ATOM_MINGW_RUNTIME_DLL}")
endif()
add_custom_command(OUTPUT "${staged_stamp}"
    ${stage_commands}
    COMMAND ${CMAKE_COMMAND} -E touch "${staged_stamp}"
    DEPENDS ${stage_depends}
    VERBATIM)
add_custom_target(Example_Homeless_Content DEPENDS "${staged_stamp}")
add_dependencies(Example_Homeless Example_Homeless_Content)

if(MINGW)
    target_link_options(Example_Homeless PRIVATE -static-libgcc -static-libstdc++ -lstdc++exp)
endif()

add_custom_target(Example_Homeless_Package
    COMMAND ${CMAKE_COMMAND} -E make_directory "${game_directory}/Packages"
    COMMAND $<TARGET_FILE:Atom_Tool_Packager> pack "${CMAKE_CURRENT_SOURCE_DIR}/Content"
        "${game_directory}/Packages/Homeless.apkg"
    DEPENDS Example_Homeless Atom_Tool_Packager ${content_files}
    VERBATIM)
