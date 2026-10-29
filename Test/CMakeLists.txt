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

atom_add_test(Atom_FS.VfsMount
    Atom_FS_VfsMount_Tests
    Filesystem/VfsMountTest.cpp
    Atom_FS)

# --- Asset ---
atom_add_test(Atom_Assets.ResourceManager
    Atom_Assets_ResourceManager_Tests
    Asset/ResourceManagerTest.cpp
    Atom_Assets)

# Batch acquire: input-order results, failure isolation, duplicate-id dedupe,
# and sequential fallback when the job system is not running.
atom_add_test(Atom_Assets.ResourceManagerBatch
    Atom_Assets_ResourceManagerBatch_Tests
    Asset/ResourceManagerBatchTest.cpp
    Atom_Assets Atom_Concurrency Atom_FS)

# Async acquire (ARCH-107): pull-model Future contract, inline fallback, shared
# in-flight load, evict-cancels-in-flight, and error propagation.
atom_add_test(Atom_Assets.ResourceManagerAsync
    Atom_Assets_ResourceManagerAsync_Tests
    Asset/ResourceManagerAsyncTest.cpp
    Atom_Assets Atom_Concurrency Atom_FS)

# Font bytes enter the async-acquire machinery (ARCH-107 P4).
atom_add_test(Atom_Text.FontFileLoader
    Atom_Text_FontFileLoader_Tests
    Asset/FontFileLoaderTest.cpp
    Atom_Text Atom_Assets Atom_Concurrency Atom_FS)

atom_add_test(Atom_Assets.ConsumerLoader
    Atom_Assets_ConsumerLoader_Tests
    Asset/ConsumerLoaderTest.cpp
    Atom_Assets Atom_Image Atom_Media Atom_Backend_Extension)

atom_add_test(Atom_Assets.ResourceId
    Atom_Assets_ResourceId_Tests
    Asset/ResourceIdTest.cpp
    Atom_Assets)

atom_add_test(Atom_Assets.ScriptSourceLoader
    Atom_Assets_ScriptSourceLoader_Tests
    Asset/ScriptSourceLoaderTest.cpp
    Atom_Assets)

# ARCH-107 Stage D: unified URI loading across directory + package mounts.
atom_add_test(Atom_Assets.StageDAcceptance
    Atom_Assets_StageDAcceptance_Tests
    Asset/StageDAcceptanceTest.cpp
    Atom_Media Atom_Image Atom_Backend_Extension Atom_FS Atom_UTF8 Atom_Packager)

# --- Render ---
atom_add_test(Atom_Render_Resources.TextureCache
    Atom_Render_Resources_TextureCache_Tests
    Render/TextureCacheTest.cpp
    Atom_Render_Resources Atom_FS)

atom_add_test(Atom_Render_Text.Font
    Atom_Render_Text_Font_Tests
    Render/FontTest.cpp
    Atom_Text)
target_compile_definitions(Atom_Render_Text_Font_Tests PRIVATE ATOM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# W2a: FreeType-backed FontProvider (family registration, instance handout,
# glyph-id mapping, metrics, on-demand rasterization) + W2b batch rasterization.
atom_add_test(Atom_Render_Text.FreeTypeFontProvider
    Atom_Render_Text_FreeTypeFontProvider_Tests
    Render/FreeTypeFontProviderTest.cpp
    Atom_Text Atom_Concurrency)
target_compile_definitions(Atom_Render_Text_FreeTypeFontProvider_Tests PRIVATE ATOM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# W2c: HarfBuzz shaping (glyph ids, byte-offset clusters, advances, mark positioning).
atom_add_test(Atom_Render_Text.HarfBuzzTextShaper
    Atom_Render_Text_HarfBuzzTextShaper_Tests
    Render/HarfBuzzTextShaperTest.cpp
    Atom_Text)
target_compile_definitions(Atom_Render_Text_HarfBuzzTextShaper_Tests PRIVATE ATOM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# W3: immutable TextLayout builder (shaping-driven line breaking, clusters, alignment, line boxes).
atom_add_test(Atom_Render_Text.TextLayoutBuilder
    Atom_Render_Text_TextLayoutBuilder_Tests
    Render/TextLayoutBuilderTest.cpp
    Atom_Text Atom_Concurrency)
target_compile_definitions(Atom_Render_Text_TextLayoutBuilder_Tests PRIVATE ATOM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

atom_add_test(Atom_Renderer2D.Text
    Atom_Renderer2D_Text_Tests
    Render/Renderer2DTextTest.cpp
    Atom_Renderer2D)
target_compile_definitions(Atom_Renderer2D_Text_Tests PRIVATE ATOM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# T1.2: Scene2D sprite collection and the 2D camera, asserted headlessly - draw
# order, destination rects, pivots, unresolved-texture handling and the
# Camera2D -> BeginFrame(origin, zoom) mapping.
atom_add_test(Atom_Scene2D.SpriteRenderer
    Atom_Scene2D_SpriteRenderer_Tests
    Render/Scene2DRendererTest.cpp
    Atom_Scene2D)

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

atom_add_test(Atom_Input.ActionMap
    Atom_Input_ActionMap_Tests
    Input/ActionMapTest.cpp
    Atom_Input)

atom_add_test(Atom_Input.InputSystem
    Atom_Input_InputSystem_Tests
    Input/InputSystemTest.cpp
    Atom_InputSystem)

# T3.2: gameplay asks for actions (axes, edges) instead of keys, and one gate suspends
# every gameplay query at once while menu input keeps working.
atom_add_test(Atom_Input.ActionState
    Atom_Input_ActionState_Tests
    Input/ActionStateTest.cpp
    Atom_InputSystem)

# --- Retained game UI -------------------------------------------------------
atom_add_test(Atom_UI.UiTree
    Atom_UI_UiTree_Tests
    UI/UiTreeTest.cpp
    Atom_UI)

# --- Unified error value (CORE-004) ---
atom_add_test(Atom_Error.Error
    Atom_Error_Error_Tests
    Error/ErrorTest.cpp
    Atom_Error)

# --- ECS (entity/component storage) ---
atom_add_test(Atom_ECS.EntityWorld
    Atom_ECS_EntityWorld_Tests
    ECS/EntityWorldTest.cpp
    Atom_ECS)

# T1.1: the frozen first batch of 2D scene components - concepts, defaults, and
# the four new query sets (a used-but-not-instantiated set fails at link time).
atom_add_test(Atom_ECS.ComponentVocabulary
    Atom_ECS_ComponentVocabulary_Tests
    ECS/ComponentVocabularyTest.cpp
    Atom_ECS)

# --- Physics (shapes + mass properties) ---
atom_add_test(Atom_Physics.Shapes
    Atom_Physics_Shapes_Tests
    Physics/ShapeTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.RigidBody
    Atom_Physics_RigidBody_Tests
    Physics/RigidBodyTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.BroadPhase
    Atom_Physics_BroadPhase_Tests
    Physics/BroadPhaseTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.InternalWorld
    Atom_Physics_InternalWorld_Tests
    Physics/InternalWorldTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.NarrowPhase
    Atom_Physics_NarrowPhase_Tests
    Physics/NarrowPhaseTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.ContactSolver
    Atom_Physics_ContactSolver_Tests
    Physics/ContactSolverTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.PhysicsWorld
    Atom_Physics_PhysicsWorld_Tests
    Physics/PhysicsWorldTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.RevoluteJoint
    Atom_Physics_RevoluteJoint_Tests
    Physics/RevoluteJointTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.Integration
    Atom_Physics_Integration_Tests
    Physics/PhysicsIntegrationTest.cpp
    Atom_Physics)

atom_add_test(Atom_Physics.Parallel
    Atom_Physics_Parallel_Tests
    Physics/PhysicsParallelTest.cpp
    Atom_Physics)

# T3.1a: the three contract additions the ECS integration needs - sensors (report but
# never resolve), a stable owner id, and persist contacts.
atom_add_test(Atom_Physics.SensorAndUserData
    Atom_Physics_SensorAndUserData_Tests
    Physics/SensorAndUserDataTest.cpp
    Atom_Physics)

# --- Backend extension registries ---
atom_add_test(Atom_Backend_Extension.AudioDecoderRegistry
    Atom_Backend_Extension_AudioDecoderRegistry_Tests
    Backend/AudioDecoderRegistryTest.cpp
    Atom_Backend_Extension)

atom_add_test(Atom_Backend_Extension.BackendRegistry
    Atom_Backend_Extension_BackendRegistry_Tests
    Backend/BackendRegistryTest.cpp
    Atom_Backend_Extension)

# ARCH-006: shared public numeric boundary contract.
atom_add_test(Atom_Backend_Contracts.NumericContract
    Atom_Backend_Contracts_NumericContract_Tests
    Backend/NumericContractTest.cpp
    Atom_Backend_Contracts)

# A0.3: SDL error capture belongs to the SDL3 backend, so Atom_Error stays
# linkable without SDL and this behaviour is covered where it lives.
atom_add_test(Atom_Backend_SDL3_Core.SdlError
    Atom_Backend_SDL3_Core_SdlError_Tests
    Backend/Sdl3ErrorTest.cpp
    Atom_Backend_SDL3_Core)

# Display selection contract: window placement, present mode, capability mask.
atom_add_test(Atom_Backend_Contracts.DisplayConfig
    Atom_Backend_Contracts_DisplayConfig_Tests
    Backend/DisplayConfigTest.cpp
    Atom_Backend_Contracts)

# Display policy: queue once, apply at the frame boundary, report the truth.
atom_add_test(Atom_Display.DisplaySystem
    Atom_Display_DisplaySystem_Tests
    Display/DisplaySystemTest.cpp
    Atom_Display)

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

# Track lifecycle: every Load() has a matching Unload(), so a long-running player
# walking a library does not pin decoders/stream buffers forever.
atom_add_test(Atom_Media.MusicPlayer
    Atom_Media_MusicPlayer_Tests
    Media/MusicPlayerTest.cpp
    Atom_Media Atom_FS)

# Batch metadata reads: input-order results + sequential fallback when the job
# system is not running. Links Atom_Concurrency explicitly because the test
# drives the scheduler's Start/Shutdown to exercise both code paths.
atom_add_test(Atom_Media.AudioMetadataReader
    Atom_Media_AudioMetadataReader_Tests
    Media/AudioMetadataReaderTest.cpp
    Atom_Media Atom_Concurrency Atom_FS)

atom_add_test(Atom_Media.Image.ColorStatistics
    Atom_Media_Image_ColorStatistics_Tests
    Media/ColorStatisticsTest.cpp
    Atom_Image)

# ARCH-006: image header guard (no unbounded allocation from a header).
atom_add_test(Atom_Media.Image.ImageDecoder
    Atom_Media_Image_ImageDecoder_Tests
    Media/ImageDecoderTest.cpp
    Atom_Image Atom_Log)

# Batch image decode: input-order results + sequential fallback when the job
# system is not running (mirrors Atom_Media.AudioMetadataReader).
atom_add_test(Atom_Media.Image.ImageBatchDecoder
    Atom_Media_Image_ImageBatchDecoder_Tests
    Media/ImageBatchDecoderTest.cpp
    Atom_Image Atom_Concurrency Atom_FS)

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

# --- Concurrency ---
atom_add_test(Atom_Concurrency.JobSystem
    Atom_Concurrency_JobSystem_Tests
    Concurrency/JobSystemTest.cpp
    Atom_Concurrency)

atom_add_test(Atom_UTF8.Utf8
    Atom_UTF8_Utf8_Tests
    Utilities/Utf8Test.cpp
    Atom_UTF8)

atom_add_test(Atom_Packager.PackageBuilder
    Atom_Packager_PackageBuilder_Tests
    Utilities/PackageBuilderTest.cpp
    Atom_Packager Atom_FS)

# --- Runtime ---
atom_add_test(Atom_Runtime.ScreenManager
    Atom_Runtime_ScreenManager_Tests
    Runtime/ScreenManagerTest.cpp
    Atom_Runtime)

atom_add_test(Atom_Runtime.GameWorld
    Atom_Runtime_GameWorld_Tests
    Runtime/GameWorldTest.cpp
    Atom_Runtime)

# T1.1: spawning 2D scene entities through the command buffer - the path T1.3's
# GameScreen uses - including the "failed setup leaves no ghost entity" rule.
atom_add_test(Atom_Runtime.CommandBufferScene
    Atom_Runtime_CommandBufferScene_Tests
    Runtime/CommandBufferSceneTest.cpp
    Atom_Runtime)

# T1.3 / G0.1: a Screen's frame (FixedUpdate 0..N + one Update) mapped onto one
# ordered GameWorld frame. Records the phase sequence the world really executes.
atom_add_test(Atom_Runtime.GameScreenFrame
    Atom_Runtime_GameScreenFrame_Tests
    Runtime/GameScreenFrameTest.cpp
    Atom_Runtime)

# T2.2: swapping a world's entity storage in one step. Systems and the frame counter
# survive; the old world's pending commands and queued events are dropped, because
# they name entities that no longer exist.
atom_add_test(Atom_Runtime.WorldReplace
    Atom_Runtime_WorldReplace_Tests
    Runtime/WorldReplaceTest.cpp
    Atom_Runtime)

# --- Json (Utilities) ---
# The JSONC reader: acceptance, plus every rejection the schema document promises,
# each pinned to a line:column. It is a general utility, so its test does not depend
# on the Scene module at all.
atom_add_test(Atom_Json.Reader
    Atom_Json_Reader_Tests
    Utilities/JsonReaderTest.cpp
    Atom_Json)

# --- Sprite (art pipeline: atlas metadata) ---
# Parsing rules, defaults and rejections, plus the shipped atlas document read through
# a real filesystem so the demo's sheet cannot rot.
atom_add_test(Atom_Sprite.Atlas
    Atom_Sprite_Atlas_Tests
    Sprite/SpriteAtlasTest.cpp
    Atom_Sprite)
target_compile_definitions(Atom_Sprite_Atlas_Tests PRIVATE ATOM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# --- Scene (T2.1 format, T2.2 loader) ---
# Schema + loader: the shipped demo room is read through a real filesystem so it
# cannot rot, the invalid fixtures under Test/Data must be rejected, and the
# component mapping is asserted field by field.
atom_add_test(Atom_Scene.SceneLoader
    Atom_Scene_SceneLoader_Tests
    Scene/SceneLoaderTest.cpp
    Atom_Scene)
target_compile_definitions(Atom_Scene_SceneLoader_Tests PRIVATE ATOM_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# Scene sprites that name an atlas frame instead of a texel rectangle: resolution, and
# the rule that the two sprite forms cannot be mixed.
atom_add_test(Atom_Scene.SceneAtlas
    Atom_Scene_SceneAtlas_Tests
    Scene/SceneAtlasTest.cpp
    Atom_Scene)

# T2.3: flat prefabs and the override whitelist. Prefabs come from a provider
# callback, so the merge logic is tested without a filesystem; the shipped prefab file
# is covered by Atom_Scene.SceneLoader through the demo room.
atom_add_test(Atom_Scene.ScenePrefab
    Atom_Scene_ScenePrefab_Tests
    Scene/ScenePrefabTest.cpp
    Atom_Scene)

# --- Lua (ARCH-005: error handling, stack balance, null injection) ---
atom_add_test(Atom_Lua.LuaHost
    Atom_Lua_LuaHost_Tests
    Lua/LuaHostTest.cpp
    Atom_Lua Atom_FS Atom_Log)

# Export list for MinGW runtime-DLL wiring in the parent CMakeLists.
set_property(GLOBAL PROPERTY ATOM_TEST_TARGETS ${ATOM_TEST_ALL_TARGETS})
