/**
  * @file           : SDL3TimeSource.hpp
  * @author         : Romi Brooks
  * @brief          : ITimeSource bound to SDL_GetPerformanceCounter.
  * @attention      : Prefer this over reimplementing QPC/mach/chrono. SDL
  *                   already normalizes the host high-resolution counter.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_BACKEND_SDL3_TIME_SDL3TIMESOURCE_HPP
#define ATOM_BACKEND_SDL3_TIME_SDL3TIMESOURCE_HPP

#include <cstdint>

#include <Backend/Contracts/Time/ITimeSource.hpp>

namespace atom::backend::sdl3 {

class SDL3TimeSource final : public ITimeSource {
    public:
        SDL3TimeSource();

        [[nodiscard]] auto NowNs() const -> std::uint64_t override;

        // Host counter frequency in Hz (0 if unavailable).
        [[nodiscard]] auto FrequencyHz() const -> std::uint64_t;

    private:
        std::uint64_t performance_frequency_ = 0;
};

} // namespace atom::backend::sdl3

#endif // ATOM_BACKEND_SDL3_TIME_SDL3TIMESOURCE_HPP
