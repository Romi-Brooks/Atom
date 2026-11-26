# Keep the in-tree dependency declarations usable with either SDK build.
function(atom_link_libraries target scope)
    if(NOT ATOM_BUILD_SHARED)
        target_link_libraries(${target} ${scope} ${ARGN})
        return()
    endif()

    set(libraries "")
    foreach(library IN LISTS ARGN)
        if(TARGET ${library})
            get_property(sdk_library TARGET ${library} PROPERTY ATOM_SDK_LIBRARY)
            if(sdk_library)
                list(APPEND libraries ${sdk_library})
                get_property(type TARGET ${target} PROPERTY TYPE)
                if(type STREQUAL "EXECUTABLE")
                    set_property(GLOBAL APPEND PROPERTY ATOM_SDK_CONSUMERS ${target})
                endif()
                continue()
            endif()
        endif()
        list(APPEND libraries ${library})
    endforeach()
    list(REMOVE_DUPLICATES libraries)
    target_link_libraries(${target} ${scope} ${libraries})
endfunction()

set(atom_sdk_cmake_dir "${CMAKE_INSTALL_DATADIR}/Atom/cmake")
set(atom_sdk_shader_dir "${CMAKE_INSTALL_DATADIR}/Atom/shaders")

if(ATOM_BUILD_SHARED)
    function(atom_add_sdk_module module component output_name)
        set(dll Atom_Sdk_${component})
        add_library(${dll} SHARED)
        target_compile_features(${dll} PUBLIC cxx_std_23)
        target_compile_definitions(${dll} PUBLIC ATOM_BUILD_SHARED
            PRIVATE ATOM_${module}_EXPORTS)
        target_include_directories(${dll} PUBLIC
            "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Engine>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
        set_target_properties(${dll} PROPERTIES OUTPUT_NAME "${output_name}" EXPORT_NAME ${component})
        if(WIN32)
            set_target_properties(${dll} PROPERTIES PREFIX "")
        elseif(APPLE)
            set_target_properties(${dll} PROPERTIES INSTALL_RPATH "@loader_path")
            target_link_options(${dll} PRIVATE "LINKER:-undefined,error")
        elseif(UNIX)
            set_target_properties(${dll} PROPERTIES INSTALL_RPATH "$ORIGIN")
            target_link_options(${dll} PRIVATE "LINKER:-z,defs")
        endif()

        foreach(target IN LISTS ATOM_${module}_MODULE_TARGETS)
            set_property(TARGET ${target} PROPERTY ATOM_SDK_LIBRARY ${dll})
            get_target_property(type ${target} TYPE)
            if(type STREQUAL "OBJECT_LIBRARY")
                target_compile_definitions(${target} PRIVATE ATOM_BUILD_SHARED)
                set_target_properties(${target} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
                # Original targets describe compilation requirements. Only this
                # DLL owns their objects and their binary link dependencies.
                target_sources(${dll} PRIVATE "$<TARGET_OBJECTS:${target}>")
            endif()
        endforeach()

        set(atom_sdk_dlls ${atom_sdk_dlls} ${dll} PARENT_SCOPE)
    endfunction()

    set(atom_sdk_dlls "")
    atom_add_sdk_module(CORE Core Atom.Core)
    atom_add_sdk_module(WORLD World Atom.World)
    atom_add_sdk_module(BACKENDS Backends Atom.Backends)
    atom_add_sdk_module(PLATFORM Platform Atom.Platform)
    atom_add_sdk_module(RUNTIME Runtime Atom.Runtime)
    atom_add_sdk_module(GRAPHICS Graphics Atom.Graphics)
    atom_add_sdk_module(MEDIA_AUDIO Audio Atom.Media.Audio)
    atom_add_sdk_module(MEDIA_IMAGE Image Atom.Media.Image)
    atom_add_sdk_module(SCRIPTING Scripting Atom.Scripting)
    atom_add_sdk_module(DEBUGGER Debugger Atom.Debugger)

    # SDL is a private runtime dependency. The installed SDK ships the loader's
    # library without exposing SDL's build target or headers to consumers.
    set(atom_sdl_link "$<BUILD_INTERFACE:$<TARGET_LINKER_FILE:Atom_3rd_SDL3>>")
    target_link_libraries(Atom_Sdk_World PUBLIC Atom_Sdk_Core)
    target_link_libraries(Atom_Sdk_Backends PUBLIC Atom_Sdk_Core PRIVATE Atom_3rd_SDL3_mixer "${atom_sdl_link}")
    target_link_libraries(Atom_Sdk_Platform PUBLIC Atom_Sdk_Core PRIVATE Atom_Sdk_Backends "${atom_sdl_link}")
    target_link_libraries(Atom_Sdk_Runtime PUBLIC Atom_Sdk_Core Atom_Sdk_Platform)
    target_link_libraries(Atom_Sdk_Graphics PUBLIC Atom_Sdk_Core Atom_Sdk_Image
        PRIVATE Atom_3rd_FreeType Atom_3rd_HarfBuzz Atom_3rd_Yoga)
    target_link_libraries(Atom_Sdk_Audio PUBLIC Atom_Sdk_Core Atom_Sdk_Backends PRIVATE Atom_3rd_TagLib)
    target_link_libraries(Atom_Sdk_Image PUBLIC Atom_Sdk_Core)
    target_link_libraries(Atom_Sdk_Scripting PUBLIC Atom_Sdk_Core PRIVATE Atom_Sdk_Audio Atom_3rd_Lua)
    target_link_libraries(Atom_Sdk_Debugger PUBLIC Atom_Sdk_Core Atom_Sdk_Platform Atom_Sdk_Runtime
        PRIVATE Atom_Sdk_Backends "${atom_sdl_link}" Atom_3rd_ImGui)
    target_sources(Atom_Sdk_Debugger PRIVATE "$<TARGET_OBJECTS:Atom_3rd_ImGui_SDL3>")
    foreach(target IN ITEMS Atom_Sdk_Backends Atom_Sdk_Platform Atom_Sdk_Debugger)
        add_dependencies(${target} Atom_3rd_SDL3)
    endforeach()

    add_library(Atom_Sdk_Media INTERFACE)
    set_target_properties(Atom_Sdk_Media PROPERTIES EXPORT_NAME Media)
    target_link_libraries(Atom_Sdk_Media INTERFACE Atom_Sdk_Audio Atom_Sdk_Image)
    get_property(atom_all_members TARGET Atom_All PROPERTY INTERFACE_LINK_LIBRARIES)
    set_property(TARGET Atom_All PROPERTY INTERFACE_LINK_LIBRARIES "")
    atom_link_libraries(Atom_All INTERFACE ${atom_all_members})
    get_property(atom_all_dlls TARGET Atom_All PROPERTY INTERFACE_LINK_LIBRARIES)
    set_property(TARGET Atom_All Atom_Core PROPERTY ATOM_SDK_LIBRARY "${atom_all_dlls}")

    target_include_directories(Atom_Sdk_Debugger INTERFACE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ThirdParty/ImGUI>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/imgui>")
    target_include_directories(Atom_Sdk_Scripting INTERFACE
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ThirdParty/Lua>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lua>")
    target_compile_definitions(Atom_Sdk_Debugger INTERFACE ATOM_ENABLE_DEBUGGER=1)

    if(WIN32)
        target_sources(Atom_Sdk_Core PRIVATE cmake/AtomCoreShutdown.cpp)
        target_sources(Atom_Sdk_Backends PRIVATE cmake/AtomBackendsShutdown.cpp)
        target_sources(Atom_Sdk_Runtime PRIVATE cmake/AtomRuntimeShutdown.cpp)
        target_link_libraries(Atom_Sdk_Runtime PRIVATE Atom_Sdk_Backends)
        target_sources(Atom_Sdk_Core INTERFACE
            "$<BUILD_INTERFACE:$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${PROJECT_SOURCE_DIR}/cmake/AtomProcessExit.cpp>>")
        target_compile_definitions(Atom_Sdk_Runtime INTERFACE
            "$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:ATOM_SDK_RUNTIME_EXIT>")

        target_compile_definitions(Atom_3rd_ImGui PUBLIC "IMGUI_API=__declspec(dllexport)")
        target_compile_definitions(Atom_3rd_Lua PRIVATE LUA_BUILD_AS_DLL)
        target_compile_definitions(Atom_Lua PRIVATE LUA_BUILD_AS_DLL LUA_CORE)
        target_compile_definitions(Atom_Sdk_Debugger INTERFACE "IMGUI_API=__declspec(dllimport)")
        target_compile_definitions(Atom_Sdk_Scripting INTERFACE LUA_BUILD_AS_DLL)
        target_link_libraries(Atom_Sdk_Debugger PRIVATE opengl32 gdi32)
    endif()

else()
    include("${CMAKE_CURRENT_LIST_DIR}/SdkArchives.cmake")
endif()

set(atom_sdk_entries Atom_Entry)
if(WIN32)
    list(APPEND atom_sdk_entries Atom_Entry_Windowed)
endif()
foreach(target IN LISTS atom_sdk_entries)
    target_include_directories(${target} INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
endforeach()
set(atom_sdk_targets ${atom_sdk_entries})
if(ATOM_BUILD_SHARED)
    list(APPEND atom_sdk_targets ${atom_sdk_dlls} Atom_Sdk_Media)
endif()

install(TARGETS ${atom_sdk_targets} EXPORT AtomTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(EXPORT AtomTargets NAMESPACE Atom:: DESTINATION "${atom_sdk_cmake_dir}")

if(ATOM_BUILD_SHARED)
    # The SDK ships SDL's runtime without its development files.
    install(TARGETS Atom_3rd_SDL3
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()
install(FILES ThirdParty/SDL3/LICENSE.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME SDL3.txt)
install(FILES ThirdParty/ImGUI/LICENSE.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME ImGui.txt)
install(FILES ThirdParty/SDL_mixer/LICENSE.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME SDL3_mixer.txt)
install(FILES ThirdParty/HarfBuzz/COPYING DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME HarfBuzz.txt)
install(FILES ThirdParty/Yoga/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME Yoga.txt)
install(FILES ThirdParty/EnTT/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME EnTT.txt)
install(FILES ThirdParty/utfcpp/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME utfcpp.txt)
install(FILES ThirdParty/minimp3/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME minimp3.txt)
install(FILES ThirdParty/stb/LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME stb.txt)
install(FILES ThirdParty/taglib/COPYING.LGPL DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME TagLib-LGPL.txt)
install(FILES ThirdParty/taglib/COPYING.MPL DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME TagLib-MPL.txt)
install(FILES ThirdParty/FreeType/docs/FTL.TXT DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME FreeType.txt)

install(DIRECTORY Engine/Atom/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Atom"
    FILES_MATCHING PATTERN "*.hpp"
    REGEX "/(Backend/(SDL3|SDLGPU|Composition|Extension)|Platform/Entry)(/|$)" EXCLUDE
    PATTERN "ProcessShutdown.hpp" EXCLUDE)
install(FILES
    ThirdParty/ImGUI/imgui.h ThirdParty/ImGUI/imgui_internal.h ThirdParty/ImGUI/imconfig.h
    ThirdParty/ImGUI/imstb_rectpack.h ThirdParty/ImGUI/imstb_textedit.h ThirdParty/ImGUI/imstb_truetype.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/imgui")
install(FILES ThirdParty/Lua/lua.h ThirdParty/Lua/lualib.h ThirdParty/Lua/lauxlib.h ThirdParty/Lua/luaconf.h
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/lua")
get_property(atom_shader_files TARGET Atom_Shaders PROPERTY ATOM_SHADER_FILES)
foreach(shader IN LISTS atom_shader_files)
    if(NOT shader MATCHES "\\.hlsl$")
        get_filename_component(variant_dir "${shader}" DIRECTORY)
        get_filename_component(variant "${variant_dir}" NAME)
        install(FILES "${shader}" DESTINATION "${atom_sdk_shader_dir}/${variant}")
    endif()
endforeach()
install(FILES README.md LICENSE DESTINATION ".")

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/AtomConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/AtomConfig.cmake"
    INSTALL_DESTINATION "${atom_sdk_cmake_dir}"
    PATH_VARS CMAKE_INSTALL_BINDIR CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR atom_sdk_shader_dir)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/AtomConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}" COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AtomConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AtomConfigVersion.cmake" cmake/AtomHelpers.cmake
    DESTINATION "${atom_sdk_cmake_dir}")
if(WIN32 AND ATOM_BUILD_SHARED)
    install(FILES cmake/AtomProcessExit.cpp DESTINATION "${atom_sdk_cmake_dir}")
endif()

option(ATOM_STAGE_SDK "Install the SDK into the build tree as part of the default build" OFF)
set(atom_sdk_default_build "")
if(ATOM_STAGE_SDK)
    set(atom_sdk_default_build ALL)
endif()

set(atom_sdk_stage_dir "${CMAKE_CURRENT_BINARY_DIR}/sdk")
get_property(atom_sdk_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(atom_sdk_multi_config)
    string(APPEND atom_sdk_stage_dir "/$<CONFIG>")
endif()
if(ATOM_BUILD_SHARED)
    string(APPEND atom_sdk_stage_dir "/shared")
    set(atom_sdk_stage_targets ${atom_sdk_entries} ${atom_sdk_dlls} Atom_3rd_SDL3 Atom_Shaders)
else()
    string(APPEND atom_sdk_stage_dir "/static")
    set(atom_sdk_stage_targets AtomSdkArchive)
endif()

add_custom_target(AtomSdk ${atom_sdk_default_build}
    COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_CURRENT_BINARY_DIR}"
        --prefix "${atom_sdk_stage_dir}" --config "$<CONFIG>"
    COMMENT "Installing the Atom SDK to ${atom_sdk_stage_dir}"
    VERBATIM)
add_dependencies(AtomSdk ${atom_sdk_stage_targets})
