cmake_minimum_required(VERSION 3.20)
project(AtomSdkSmoke LANGUAGES CXX)

find_package(Atom CONFIG REQUIRED COMPONENTS
    Core World Backends Platform Runtime Graphics Media Audio Image Scripting Debugger Entry)

add_executable(HelloAtom HelloAtom.cpp)
target_link_libraries(HelloAtom PRIVATE Atom::Core)
atom_application(HelloAtom CONSOLE)

add_executable(AtomCustomMain CustomMain.cpp)
target_link_libraries(AtomCustomMain PRIVATE Atom::Runtime)

add_executable(AtomSdkGraphics Graphics.cpp)
target_link_libraries(AtomSdkGraphics PRIVATE Atom::Graphics)

add_executable(AtomSdkLua LuaBinding.cpp)
target_link_libraries(AtomSdkLua PRIVATE Atom::Scripting)

add_executable(AtomSdkPanel DebugPanel.cpp)
target_link_libraries(AtomSdkPanel PRIVATE Atom::Debugger)

# Compile each installed header in a separate translation unit. The only
# Atom include directories are the ones advertised by the installed targets.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS RELATIVE "${Atom_INCLUDE_DIR}" "${Atom_INCLUDE_DIR}/Atom/*.hpp")
set(audit_sources "")
foreach(header IN LISTS headers)
    string(REPLACE "/" "_" filename "${header}.cpp")
    set(source "${CMAKE_CURRENT_BINARY_DIR}/headers/${filename}")
    file(GENERATE OUTPUT "${source}" CONTENT "#include <${header}>\n")
    list(APPEND audit_sources "${source}")
endforeach()
add_library(AtomSdkHeaderCheck OBJECT ${audit_sources})
target_link_libraries(AtomSdkHeaderCheck PRIVATE Atom::Core Atom::Debugger Atom::Scripting)

enable_testing()
if(WIN32)
    file(GLOB runtime_dlls CONFIGURE_DEPENDS "${Atom_RUNTIME_DIR}/*.dll")
    add_custom_target(StageAtomSdkRuntime
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:HelloAtom>"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_dlls} "$<TARGET_FILE_DIR:HelloAtom>"
        VERBATIM)
    foreach(target IN ITEMS HelloAtom AtomCustomMain AtomSdkGraphics AtomSdkLua AtomSdkPanel)
        add_dependencies(${target} StageAtomSdkRuntime)
    endforeach()
endif()
foreach(target IN ITEMS HelloAtom AtomCustomMain)
    add_test(NAME ${target} COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:${target}>"
        "-DCHECK_SCREEN=$<AND:$<PLATFORM_ID:Windows>,$<STREQUAL:${target},AtomCustomMain>>" -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckProgram.cmake")
    set_tests_properties(${target} PROPERTIES TIMEOUT 15)
endforeach()
foreach(target IN ITEMS AtomSdkGraphics AtomSdkLua AtomSdkPanel)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 15)
endforeach()
