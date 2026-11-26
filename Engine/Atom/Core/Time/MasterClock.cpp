/**
  * @file           : MasterClock.cpp
  * @author         : Romi Brooks
  * @brief          : Monotonic host clock with a stable epoch for domain clocks.
  * @attention      :
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "MasterClock.hpp"

namespace atom::time {

MasterClock::MasterClock(const backend::ITimeSource& source) : source_(&source) {
    Reset();
}

auto MasterClock::Reset() -> void {
    const auto now = HostNowNs();
    epoch_host_ns_ = now;
    last_host_ns_ = now;
    last_delta_ns_ = 0;
}

auto MasterClock::HostNowNs() const -> std::uint64_t {
    return source_ ? source_->NowNs() : 0;
}

auto MasterClock::Now() const -> TimePoint {
    const auto host = HostNowNs();
    return host >= epoch_host_ns_ ? static_cast<TimePoint>(host - epoch_host_ns_) : 0;
}

auto MasterClock::Delta() const -> Duration {
    return last_delta_ns_;
}

auto MasterClock::ObserveHost() -> Duration {
    const auto host = HostNowNs();
    last_delta_ns_ = host >= last_host_ns_ ? static_cast<Duration>(host - last_host_ns_) : 0;
    last_host_ns_ = host;
    return last_delta_ns_;
}

} // namespace atom::time
