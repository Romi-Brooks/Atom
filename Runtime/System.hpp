/**
 * @file System.hpp
 * @brief Explicit, deterministic game-system scheduling contracts.
 *
 * Runtime owns frame orchestration. Game systems deliberately do not register
 * anonymous callbacks on Application: their phase and registration order are
 * part of the simulation contract and are therefore visible and testable.
 */
#ifndef ATOM_RUNTIME_SYSTEM_HPP
#define ATOM_RUNTIME_SYSTEM_HPP

#include <cstdint>
#include <functional>
#include <string>

#include <Error/Error.hpp>

namespace atom::runtime {

class GameWorld;

enum class SystemPhase : std::uint8_t {
    Input,
    FixedUpdate,
    Update,
    LateUpdate,
};

struct FrameContext final {
        SystemPhase phase = SystemPhase::Update;
        float delta_seconds = 0.0f;
        std::uint64_t frame_index = 0;
        std::uint32_t fixed_step_index = 0;
};

using SystemCallback = std::function<Result<void>(GameWorld&, const FrameContext&)>;

struct SystemDescriptor final {
        std::string name{};
        SystemCallback callback{};
};

} // namespace atom::runtime

#endif // ATOM_RUNTIME_SYSTEM_HPP
