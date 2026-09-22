/**
  * @file           : TimeSystem.hpp
  * @author         : Romi Brooks
  * @brief          : Facade owning the master clock and named domain clocks.
  * @attention      : Bound to a backend ITimeSource at Initialize. Domain
  *                   modules create their own DomainClock here instead of
  *                   sampling host time themselves.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_TIME_TIMESYSTEM_HPP
#define ATOM_TIME_TIMESYSTEM_HPP

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Backend/Contracts/Time/ITimeSource.hpp>
#include <Time/DomainClock.hpp>
#include <Time/MasterClock.hpp>
#include <Time/TimeTypes.hpp>

namespace atom::time {

// Well-known domain names created by CreateDefaultDomains().
namespace domain {
inline constexpr std::string_view kGame = "game";       // fixed-step gameplay / screen sim
inline constexpr std::string_view kPhysics = "physics"; // independent fixed-step world
inline constexpr std::string_view kRender = "render";   // variable visual time
inline constexpr std::string_view kUi = "ui";           // real-time UI / debugger
inline constexpr std::string_view kAudio = "audio";     // latency-compensated media
} // namespace domain

struct DefaultDomainConfig {
        Duration game_fixed_step = kSecond / 60;
        Duration physics_fixed_step = kSecond / 120;
        std::uint32_t max_steps_per_advance = 5;
};

class TimeSystem {
    public:
        [[nodiscard]] static auto GetInstance() -> TimeSystem&;

        TimeSystem(const TimeSystem&) = delete;
        auto operator=(const TimeSystem&) -> TimeSystem& = delete;

        // Bind the host source (SDL3TimeSource in production). Resets the epoch.
        auto Initialize(const backend::ITimeSource& source, const DefaultDomainConfig& config = {}) -> void;
        auto Shutdown() -> void;
        [[nodiscard]] auto IsInitialized() const -> bool;

        [[nodiscard]] auto Master() -> MasterClock&;
        [[nodiscard]] auto Master() const -> const MasterClock&;

        auto CreateDomain(std::string_view name, const ClockDesc& desc) -> DomainClock&;
        [[nodiscard]] auto FindDomain(std::string_view name) -> DomainClock*;
        [[nodiscard]] auto FindDomain(std::string_view name) const -> const DomainClock*;

        // Advance master + every domain once per host frame (CORE-001 tick).
        auto Tick() -> FrameTick;
        [[nodiscard]] auto LastFrame() const -> const FrameTick&;

    private:
        TimeSystem() = default;

        auto CreateDefaultDomains(const DefaultDomainConfig& config) -> void;

        std::unique_ptr<MasterClock> master_;
        std::unordered_map<std::string, DomainClock> domains_;
        FrameTick last_frame_{};
        bool initialized_ = false;
};

} // namespace atom::time

#endif // ATOM_TIME_TIMESYSTEM_HPP
