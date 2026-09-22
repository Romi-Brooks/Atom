/**
  * @file           : MasterClock.hpp
  * @author         : Romi Brooks
  * @brief          : Monotonic host clock with a stable epoch for domain clocks.
  * @attention      : Never paused or scaled. Game time lives on DomainClock.
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_TIME_MASTERCLOCK_HPP
#define ATOM_TIME_MASTERCLOCK_HPP

#include <cstdint>

#include <Backend/Contracts/Time/ITimeSource.hpp>
#include <Time/TimeTypes.hpp>

namespace atom::time {

class MasterClock {
    public:
        explicit MasterClock(const backend::ITimeSource& source);

        // Re-anchor the epoch to the current host sample (Now() becomes 0).
        auto Reset() -> void;

        // Raw host nanoseconds from the bound ITimeSource.
        [[nodiscard]] auto HostNowNs() const -> std::uint64_t;

        // Nanoseconds since Reset()/construction.
        [[nodiscard]] auto Now() const -> TimePoint;

        // Delta between the two most recent HostNowNs() observations made via
        // ObserveHost(); zero before the second observation.
        [[nodiscard]] auto Delta() const -> Duration;

        // Sample host time and update Delta(). Called once per frame.
        auto ObserveHost() -> Duration;

    private:
        const backend::ITimeSource* source_ = nullptr;
        std::uint64_t epoch_host_ns_ = 0;
        std::uint64_t last_host_ns_ = 0;
        Duration last_delta_ns_ = 0;
};

} // namespace atom::time

#endif // ATOM_TIME_MASTERCLOCK_HPP
