#ifndef ATOM_BACKEND_SDL3_SUBSYSTEM_LEASE_HPP
#define ATOM_BACKEND_SDL3_SUBSYSTEM_LEASE_HPP

namespace atom::backend::sdl3 {

enum class SDLSubsystem { Video, Audio, Events };

class SDLSubsystemLease final {
    public:
        SDLSubsystemLease() = default;
        explicit SDLSubsystemLease(SDLSubsystem subsystem);
        ~SDLSubsystemLease();
        SDLSubsystemLease(const SDLSubsystemLease&) = delete;
        auto operator=(const SDLSubsystemLease&) -> SDLSubsystemLease& = delete;
        SDLSubsystemLease(SDLSubsystemLease&& other) noexcept;
        auto operator=(SDLSubsystemLease&& other) noexcept -> SDLSubsystemLease&;
        auto Acquire(SDLSubsystem subsystem) -> bool;
        auto Reset() -> void;
        [[nodiscard]] auto IsValid() const -> bool;

    private:
        SDLSubsystem subsystem_ = SDLSubsystem::Events;
        bool valid_ = false;
};

} // namespace atom::backend::sdl3

#endif
