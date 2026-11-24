# Shared color value and operations (header-only, no renderer dependency).
# Lives under Engine/Core/Algorithm/ as a pure value/math type, not a root subsystem.
add_library(Atom_Color INTERFACE)
target_sources(Atom_Color INTERFACE
    Engine/Core/Algorithm/Color/Color.hpp
    Engine/Core/Algorithm/Color/ColorMath.hpp
)
target_include_directories(Atom_Color INTERFACE "${PROJECT_SOURCE_DIR}/Engine/Core")

# Math — pure value/math types with no engine dependency at all (A0.2).
#     Split out of Atom_Algorithm so the backend contracts can use Vec2/Mat3/Rect
#     without depending on the higher "algorithm" module (transforms, cameras).
#     Atom_Algorithm keeps the behaviour-bearing types and links this one.
#     Files stay under Engine/Core/Algorithm/ because that is where they have always lived;
#     the ownership move is a target-graph change, not a directory move.
add_library(Atom_Math STATIC
    Engine/Core/Algorithm/Vector/Vec2.cpp
    Engine/Core/Algorithm/Vector/Vec2.hpp
    Engine/Core/Algorithm/Vector/Vec3.cpp
    Engine/Core/Algorithm/Vector/Vec3.hpp
    Engine/Core/Algorithm/Vector/Vec4.cpp
    Engine/Core/Algorithm/Vector/Vec4.hpp
    Engine/Core/Algorithm/Matrix/Mat3.cpp
    Engine/Core/Algorithm/Matrix/Mat3.hpp
    Engine/Core/Algorithm/Matrix/Mat4.cpp
    Engine/Core/Algorithm/Matrix/Mat4.hpp
    Engine/Core/Algorithm/Geometry/Primitives.hpp
    Engine/Core/Algorithm/Geometry/Rect.hpp
    Engine/Core/Algorithm/Math/Scalar.hpp
    Engine/Core/Algorithm/Interpolation/Easing.hpp
)
target_include_directories(Atom_Math PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")
# Color is a header-only value type; Vec2/Mat3 do not use it, but keeping the
# edge here means every math consumer gets the full value vocabulary.
target_link_libraries(Atom_Math PUBLIC Atom_Color)

# Deterministic pseudorandom streams for simulations and tools.
add_library(Atom_Random STATIC
    Engine/Core/Utilities/Random/Rng.hpp
    Engine/Core/Utilities/Random/Rng.cpp
)
target_include_directories(Atom_Random PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")

# Error — the engine's unified failure value (CORE-004).
#     Leaf module: no engine dependency at all, not even Log. Every other
#     module may link it, and it must never link anything back. Defined before
#     Log because Log consumes Error codes for its own diagnostics.
add_library(Atom_Error STATIC
    Engine/Core/Error/Error.hpp
    Engine/Core/Error/Error.cpp
)
target_include_directories(Atom_Error PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")
# A0.3: Atom_Error is platform-free by contract. It must stay linkable
# without any backend compiled in, so SDL error capture lives in
# Engine/Backend/SDL3/Core/SdlError.hpp instead of being exported from here. This
# is what lets Engine/Resources/Filesystem/Asset/ECS/Scene tests link with no SDL backend.

# Time (master + domain clocks, fixed-step policy)
add_library(Atom_Time STATIC
        Engine/Core/Time/TimeTypes.hpp
        Engine/Core/Time/SteadyTimeSource.hpp
        Engine/Core/Time/MasterClock.hpp
        Engine/Core/Time/MasterClock.cpp
        Engine/Core/Time/DomainClock.hpp
        Engine/Core/Time/DomainClock.cpp
        Engine/Core/Time/TimeSystem.hpp
        Engine/Core/Time/TimeSystem.cpp
)
target_include_directories(Atom_Time PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")
target_link_libraries(Atom_Time PUBLIC Atom_Backend_Contracts)

# ThreadRegistry — process-wide registry of long-lived threads for
#     diagnostics (which thread is alive, which one burns the most CPU). A leaf
#     module with no dependency on Log or Concurrency, so Log's own writer
#     thread can register without a cycle.
add_library(Atom_Threading STATIC
    Engine/Core/Concurrency/ThreadRegistry.hpp
    Engine/Core/Concurrency/ThreadRegistry.cpp
)
target_include_directories(Atom_Threading PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")

# Concurrency — work-stealing job scheduler over a fixed worker pool.
#     Standalone foundation with no dependency on Engine/Core/Log/Time/backend; every
#     subsystem (audio decode, asset loading, physics, AI) can schedule work
#     through it. Header-only deque + job types, with a compiled scheduler.
add_library(Atom_Concurrency STATIC
    Engine/Core/Concurrency/Job.hpp
    Engine/Core/Concurrency/WorkStealingDeque.hpp
    Engine/Core/Concurrency/JobSystem.hpp
    Engine/Core/Concurrency/JobSystem.cpp
    Engine/Core/Concurrency/Future.hpp
)
target_include_directories(Atom_Concurrency PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")
target_link_libraries(Atom_Concurrency PUBLIC Atom_Threading)

# Log
add_library(Atom_Log STATIC
        Engine/Core/Log/LogSystem.cpp
        Engine/Core/Log/LogSystem.hpp
        Engine/Core/Log/LogChannelMacros.hpp
        Engine/Core/Log/AtomLogChannels.hpp
)
target_include_directories(Atom_Log PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")
target_compile_definitions(Atom_Log PRIVATE
    ATOM_LOG_CONSOLE_DEFAULT=$<OR:$<CONFIG:Debug,RelWithDebInfo>,$<STREQUAL:${ATOM_ENABLE_DEBUGGER},ON>>)
# A0.2: Log's headers include no backend contract. The previous
# "PUBLIC Atom_Backend_Contracts" here was a stale edge that pulled the whole
# contract (and now Engine/Resources/Filesystem) into every Log consumer, ECS included.
target_link_libraries(Atom_Log PUBLIC Atom_Threading)

# The job scheduler logs lifecycle + swallowed-exception diagnostics, so it
# depends on the log runtime. PUBLIC so any JobSystem consumer (e.g. the
# standalone tour example) links the log symbols transitively.
target_link_libraries(Atom_Concurrency PUBLIC Atom_Log)

# Algorithm — behaviour-bearing math (transforms, cameras).
#     A0.2: pure values moved to Atom_Math. This target used to link
#     Atom_Backend_Contracts although not one of its headers includes anything
#     from Engine/Backend/, which is what closed the Algorithm <-> Contracts cycle.
add_library(Atom_Algorithm STATIC
        Engine/Core/Algorithm/Transform/Transform2D.cpp
        Engine/Core/Algorithm/Transform/Transform2D.hpp
        Engine/Core/Algorithm/Transform/Transform3D.cpp
        Engine/Core/Algorithm/Transform/Transform3D.hpp
        Engine/Core/Algorithm/Camera/Camera2D.cpp
        Engine/Core/Algorithm/Camera/Camera2D.hpp
        Engine/Core/Algorithm/Camera/Camera3D.cpp
        Engine/Core/Algorithm/Camera/Camera3D.hpp
)
target_include_directories(Atom_Algorithm PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")
target_link_libraries(Atom_Algorithm PUBLIC Atom_Math)

# Json — a strict JSONC reader, owned by Utilities because every document format
#     needs one (scenes today; sprite metadata, prefabs and save data next). A leaf:
#     standard library + Error only, so any layer may use it.
add_library(Atom_Json STATIC
    Engine/Core/Utilities/Json/JsonReader.hpp
    Engine/Core/Utilities/Json/JsonReader.cpp
)
target_include_directories(Atom_Json PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")
target_link_libraries(Atom_Json PUBLIC Atom_Error)

# UTF-8 encoding utilities
add_library(Atom_UTF8 STATIC
    Engine/Core/Utilities/Utf8/Utf8.hpp
    Engine/Core/Utilities/Utf8/Utf8.cpp
)
target_include_directories(Atom_UTF8 PUBLIC "${PROJECT_SOURCE_DIR}/Engine/Core")
# A0.2: Utf8 includes no backend contract. The stale edge to
# Atom_Backend_Contracts used to make Filesystem -> Utf8 -> Contracts a cycle
# once the contracts declared their real Filesystem dependency below.
target_link_libraries(Atom_UTF8 PUBLIC Atom_3rd_UtfCpp)
