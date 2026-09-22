/**
  * @file           : SDL3TimeSource.cpp
  * @author         : Romi Brooks
  * @brief          : ITimeSource bound to SDL_GetPerformanceCounter.
  * @attention      :
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "SDL3TimeSource.hpp"

#include <SDL3/SDL.h>

namespace atom::backend::sdl3 {

SDL3TimeSource::SDL3TimeSource() : performance_frequency_(SDL_GetPerformanceFrequency()) {}

auto SDL3TimeSource::NowNs() const -> std::uint64_t {
    if (performance_frequency_ == 0)
        return 0;
    const auto counter = SDL_GetPerformanceCounter();
    // Split whole seconds so counter * 1e9 never overflows uint64.
    const auto whole = counter / performance_frequency_;
    const auto frac = counter % performance_frequency_;
    return whole * 1000000000ull + (frac * 1000000000ull) / performance_frequency_;
}

auto SDL3TimeSource::FrequencyHz() const -> std::uint64_t {
    return performance_frequency_;
}

} // namespace atom::backend::sdl3
