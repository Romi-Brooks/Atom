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

if(NOT ATOM_BUILD_SHARED)
    return()
endif()

set(atom_sdk_cmake_dir "${CMAKE_INSTALL_DATADIR}/Atom/cmake")
set(atom_sdk_shader_dir "${CMAKE_INSTALL_DATADIR}/Atom/shaders")
set(atom_sdk_components Core World Backends Platform Runtime Graphics Media Audio Image Scripting Debugger)
set(atom_sdk_objects "")
set(atom_sdk_export_definitions "")
foreach(module IN LISTS ATOM_SDK_MODULES)
    list(APPEND atom_sdk_export_definitions ATOM_${module}_EXPORTS)
    foreach(target IN LISTS ATOM_${module}_MODULE_TARGETS)
        set_property(TARGET ${target} PROPERTY ATOM_SDK_LIBRARY Atom_Sdk)
        get_target_property(type ${target} TYPE)
        if(type STREQUAL "OBJECT_LIBRARY")
            list(APPEND atom_sdk_objects ${target})
            set_target_properties(${target} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
        endif()
    endforeach()
endforeach()
foreach(target IN LISTS atom_sdk_objects)
    target_compile_definitions(${target} PRIVATE ATOM_BUILD_SHARED ${atom_sdk_export_definitions})
endforeach()
set_property(TARGET Atom_All Atom_Core PROPERTY ATOM_SDK_LIBRARY Atom_Sdk)
set_property(TARGET Atom_All PROPERTY INTERFACE_LINK_LIBRARIES Atom_Sdk)

add_library(Atom_Sdk SHARED)
target_link_libraries(Atom_Sdk PRIVATE ${atom_sdk_objects} Atom_3rd_Lua Atom_3rd_ImGui)
target_compile_features(Atom_Sdk PUBLIC cxx_std_23)
target_compile_definitions(Atom_Sdk INTERFACE ATOM_BUILD_SHARED ATOM_ENABLE_DEBUGGER=$<BOOL:${_atom_debugger_enabled}>)
target_include_directories(Atom_Sdk INTERFACE
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Engine>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ThirdParty/ImGUI>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ThirdParty/Lua>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
set_target_properties(Atom_Sdk PROPERTIES OUTPUT_NAME Atom EXPORT_NAME Sdk)

if(WIN32)
    set_target_properties(Atom_Sdk PROPERTIES PREFIX "")
    target_sources(Atom_Sdk PRIVATE cmake/AtomShutdown.cpp)
    target_compile_definitions(Atom_Sdk PRIVATE ATOM_BUILD_SHARED ${atom_sdk_export_definitions})
    target_sources(Atom_Sdk INTERFACE
        "$<BUILD_INTERFACE:$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:${PROJECT_SOURCE_DIR}/cmake/AtomProcessExit.cpp>>")

    # Atom.dll owns the vendored Lua and ImGui implementations in this build.
    # Lua's C sources already select LUA_CORE or LUA_LIB themselves.
    target_compile_definitions(Atom_3rd_ImGui PUBLIC "IMGUI_API=__declspec(dllexport)")
    target_compile_definitions(Atom_3rd_Lua PRIVATE LUA_BUILD_AS_DLL)
    target_compile_definitions(Atom_Lua PRIVATE LUA_BUILD_AS_DLL LUA_CORE)
    target_compile_definitions(Atom_Sdk INTERFACE
        "$<BUILD_INTERFACE:IMGUI_API=__declspec(dllimport)>"
        "$<BUILD_INTERFACE:LUA_BUILD_AS_DLL>")
elseif(APPLE)
    set_target_properties(Atom_Sdk PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set_target_properties(Atom_Sdk PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

set(atom_sdk_targets Atom_Sdk Atom_Entry)
if(WIN32)
    list(APPEND atom_sdk_targets Atom_Entry_Windowed)
endif()
foreach(target IN LISTS atom_sdk_targets)
    if(NOT target STREQUAL "Atom_Sdk")
        target_include_directories(${target} INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
    endif()
endforeach()
foreach(component IN LISTS atom_sdk_components)
    set(target Atom_Sdk_${component})
    add_library(${target} INTERFACE)
    target_link_libraries(${target} INTERFACE Atom_Sdk)
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${component})
    list(APPEND atom_sdk_targets ${target})
endforeach()
target_include_directories(Atom_Sdk_Debugger INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/imgui>")
target_include_directories(Atom_Sdk_Scripting INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/lua>")
if(WIN32)
    target_compile_definitions(Atom_Sdk_Debugger INTERFACE "$<INSTALL_INTERFACE:IMGUI_API=__declspec(dllimport)>")
    target_compile_definitions(Atom_Sdk_Scripting INTERFACE "$<INSTALL_INTERFACE:LUA_BUILD_AS_DLL>")
endif()

install(TARGETS ${atom_sdk_targets} EXPORT AtomTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(EXPORT AtomTargets NAMESPACE Atom:: DESTINATION "${atom_sdk_cmake_dir}")

# Consumers load SDL through Atom. The SDK only needs SDL's runtime files.
install(TARGETS Atom_3rd_SDL3
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES ThirdParty/SDL3/LICENSE.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME SDL3.txt)
install(FILES ThirdParty/ImGUI/LICENSE.txt DESTINATION "${CMAKE_INSTALL_DATADIR}/Atom/licenses" RENAME ImGui.txt)

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
    PATH_VARS CMAKE_INSTALL_BINDIR CMAKE_INSTALL_INCLUDEDIR atom_sdk_shader_dir)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AtomConfig.cmake" cmake/AtomHelpers.cmake
    DESTINATION "${atom_sdk_cmake_dir}")
if(WIN32)
    install(FILES cmake/AtomProcessExit.cpp DESTINATION "${atom_sdk_cmake_dir}")
endif()
