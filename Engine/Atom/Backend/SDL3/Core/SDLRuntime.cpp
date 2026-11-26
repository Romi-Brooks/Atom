#include "SDLRuntime.hpp"

#include <array>
#include <cstddef>
#include <mutex>
#include <utility>
#include <SDL3/SDL.h>

namespace atom::backend::sdl3 {
namespace {
std::mutex g_subsystem_mutex;
std::array<std::size_t, 3> g_subsystem_ref_counts{};

auto ToIndex(const SDLSubsystem subsystem) -> std::size_t {
    switch (subsystem) {
    case SDLSubsystem::Video:
        return 0;
    case SDLSubsystem::Audio:
        return 1;
    case SDLSubsystem::Events:
        return 2;
    }
    return 0;
}

auto ToSDLFlag(const SDLSubsystem subsystem) -> SDL_InitFlags {
    switch (subsystem) {
    case SDLSubsystem::Video:
        return SDL_INIT_VIDEO;
    case SDLSubsystem::Audio:
        return SDL_INIT_AUDIO;
    case SDLSubsystem::Events:
        return SDL_INIT_EVENTS;
    }
    return 0;
}
} // namespace

SDLSubsystemLease::SDLSubsystemLease(const SDLSubsystem subsystem) {
    Acquire(subsystem);
}
SDLSubsystemLease::~SDLSubsystemLease() {
    Reset();
}
SDLSubsystemLease::SDLSubsystemLease(SDLSubsystemLease&& other) noexcept
    : subsystem_(other.subsystem_), valid_(std::exchange(other.valid_, false)) {}

auto SDLSubsystemLease::operator=(SDLSubsystemLease&& other) noexcept -> SDLSubsystemLease& {
    if (this == &other)
        return *this;
    Reset();
    subsystem_ = other.subsystem_;
    valid_ = std::exchange(other.valid_, false);
    return *this;
}

auto SDLSubsystemLease::Acquire(const SDLSubsystem subsystem) -> bool {
    Reset();
    std::scoped_lock lock(g_subsystem_mutex);
    const auto index = ToIndex(subsystem);
    if (g_subsystem_ref_counts[index] == 0 && !SDL_InitSubSystem(ToSDLFlag(subsystem)))
        return false;
    ++g_subsystem_ref_counts[index];
    subsystem_ = subsystem;
    valid_ = true;
    return true;
}

auto SDLSubsystemLease::Reset() -> void {
    if (!valid_)
        return;
    std::scoped_lock lock(g_subsystem_mutex);
    const auto index = ToIndex(subsystem_);
    if (g_subsystem_ref_counts[index] > 0 && --g_subsystem_ref_counts[index] == 0) {
        SDL_QuitSubSystem(ToSDLFlag(subsystem_));
    }
    valid_ = false;
}

auto SDLSubsystemLease::IsValid() const -> bool {
    return valid_;
}
} // namespace atom::backend::sdl3
