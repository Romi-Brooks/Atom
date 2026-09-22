# Centralized Atom-owned unit tests.
# Layout: Test/<Module>/*Test.cpp
# One executable + one CTest name per test file.
# Naming: Atom_<Module>.<Area> (matches historical CTest names).

set(ATOM_TEST_ALL_TARGETS "")

function(atom_add_test test_name target_name source)
    add_executable(${target_name} ${source})
    target_link_libraries(${target_name} PRIVATE ${ARGN})
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    # Allow #include <Test/...> helpers via project root.
    target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR})
    add_test(NAME ${test_name} COMMAND ${target_name})
    set(ATOM_TEST_ALL_TARGETS ${ATOM_TEST_ALL_TARGETS} ${target_name} PARENT_SCOPE)
endfunction()

# --- Filesystem (existing coverage, moved from module-local Test/) ---
atom_add_test(Atom_FS.AssetPath
    Atom_FS_AssetPath_Tests
    Filesystem/AssetPathTest.cpp
    Atom_FS)

atom_add_test(Atom_FS.NativeFileSystem
    Atom_FS_NativeFileSystem_Tests
    Filesystem/NativeFileSystemTest.cpp
    Atom_FS)

atom_add_test(Atom_FS.MemoryFileSystem
    Atom_FS_MemoryFileSystem_Tests
    Filesystem/MemoryFileSystemTest.cpp
    Atom_FS)

atom_add_test(Atom_FS.OverlayVfs
    Atom_FS_OverlayVfs_Tests
    Filesystem/OverlayVfsTest.cpp
    Atom_FS)

atom_add_test(Atom_FS.PackageFileSystem
    Atom_FS_PackageFileSystem_Tests
    Filesystem/PackageFileSystemTest.cpp
    Atom_FS)

atom_add_test(Atom_FS.VfsMount
    Atom_FS_VfsMount_Tests
    Filesystem/VfsMountTest.cpp
    Atom_FS)

# --- Asset ---
atom_add_test(Atom_Assets.ResourceManager
    Atom_Assets_ResourceManager_Tests
    Asset/ResourceManagerTest.cpp
    Atom_Assets)

atom_add_test(Atom_Assets.ConsumerLoader
    Atom_Assets_ConsumerLoader_Tests
    Asset/ConsumerLoaderTest.cpp
    Atom_Assets Atom_Image)

atom_add_test(Atom_Assets.ResourceId
    Atom_Assets_ResourceId_Tests
    Asset/ResourceIdTest.cpp
    Atom_Assets)

atom_add_test(Atom_Assets.ScriptSourceLoader
    Atom_Assets_ScriptSourceLoader_Tests
    Asset/ScriptSourceLoaderTest.cpp
    Atom_Assets)

# --- Render ---
atom_add_test(Atom_Render_Resources.TextureCache
    Atom_Render_Resources_TextureCache_Tests
    Render/TextureCacheTest.cpp
    Atom_Render_Resources Atom_FS)

# --- Algorithm / Color / Event (header-heavy pure logic) ---
atom_add_test(Atom_Algorithm.Scalar
    Atom_Algorithm_Scalar_Tests
    Algorithm/ScalarTest.cpp
    Atom_Algorithm)

atom_add_test(Atom_Algorithm.Vec2
    Atom_Algorithm_Vec2_Tests
    Algorithm/Vec2Test.cpp
    Atom_Algorithm)

atom_add_test(Atom_Algorithm.Mat3
    Atom_Algorithm_Mat3_Tests
    Algorithm/Mat3Test.cpp
    Atom_Algorithm)

atom_add_test(Atom_Algorithm.Easing
    Atom_Algorithm_Easing_Tests
    Algorithm/EasingTest.cpp
    Atom_Algorithm)

atom_add_test(Atom_Algorithm.Rect
    Atom_Algorithm_Rect_Tests
    Algorithm/RectTest.cpp
    Atom_Algorithm)

atom_add_test(Atom_Color.ColorMath
    Atom_Color_ColorMath_Tests
    Algorithm/ColorMathTest.cpp
    Atom_Color)

atom_add_test(Atom_Event.ActionMap
    Atom_Event_ActionMap_Tests
    Event/ActionMapTest.cpp
    Atom_Event)

# --- Backend extension registries ---
atom_add_test(Atom_Backend_Extension.AudioDecoderRegistry
    Atom_Backend_Extension_AudioDecoderRegistry_Tests
    Backend/AudioDecoderRegistryTest.cpp
    Atom_Backend_Extension)

atom_add_test(Atom_Backend_Extension.BackendRegistry
    Atom_Backend_Extension_BackendRegistry_Tests
    Backend/BackendRegistryTest.cpp
    Atom_Backend_Extension)

# --- Media / Image / Layout / Log / Utilities ---
atom_add_test(Atom_Media.AudioMixer
    Atom_Media_AudioMixer_Tests
    Media/AudioMixerTest.cpp
    Atom_Media)

atom_add_test(Atom_Media.VoicePool
    Atom_Media_VoicePool_Tests
    Media/VoicePoolTest.cpp
    Atom_Media)

atom_add_test(Atom_Media.MusicCrossfade
    Atom_Media_MusicCrossfade_Tests
    Media/MusicCrossfadeTest.cpp
    Atom_Media)

atom_add_test(Atom_Media.Image.ColorStatistics
    Atom_Media_Image_ColorStatistics_Tests
    Media/ColorStatisticsTest.cpp
    Atom_Image)

atom_add_test(Atom_Layout.LayoutTree
    Atom_Layout_LayoutTree_Tests
    Layout/LayoutTreeTest.cpp
    Atom_Layout)

atom_add_test(Atom_Log.LogSystem
    Atom_Log_LogSystem_Tests
    Log/LogSystemTest.cpp
    Atom_Log)

atom_add_test(Atom_Time.MasterClock
    Atom_Time_MasterClock_Tests
    Time/MasterClockTest.cpp
    Atom_Time)

atom_add_test(Atom_Time.DomainClock
    Atom_Time_DomainClock_Tests
    Time/DomainClockTest.cpp
    Atom_Time)

atom_add_test(Atom_Time.TimeSystem
    Atom_Time_TimeSystem_Tests
    Time/TimeSystemTest.cpp
    Atom_Time)

atom_add_test(Atom_UTF8.Utf8
    Atom_UTF8_Utf8_Tests
    Utilities/Utf8Test.cpp
    Atom_UTF8)

atom_add_test(Atom_Packager.RoundTrip
    Atom_Packager_RoundTrip_Tests
    Utilities/PackagerTest.cpp
    Atom_Packager)

# --- Window ---
atom_add_test(Atom_Window.ScreenManager
    Atom_Window_ScreenManager_Tests
    Window/ScreenManagerTest.cpp
    Atom_Window)

# Export list for MinGW runtime-DLL wiring in the parent CMakeLists.
set_property(GLOBAL PROPERTY ATOM_TEST_TARGETS ${ATOM_TEST_ALL_TARGETS})
