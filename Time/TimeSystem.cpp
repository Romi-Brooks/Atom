/**
  * @file           : TimeSystem.cpp
  * @author         : Romi Brooks
  * @brief          : Facade owning the master clock and named domain clocks.
  * @attention      :
  * @date           : 2026/9/12
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "TimeSystem.hpp"

namespace atom::time {

auto TimeSystem::GetInstance() -> TimeSystem& {
    static TimeSystem instance;
    return instance;
}

auto TimeSystem::Initialize(const backend::ITimeSource& source, const DefaultDomainConfig& config) -> void {
    master_ = std::make_unique<MasterClock>(source);
    domains_.clear();
    CreateDefaultDomains(config);
    last_frame_ = FrameTick{};
    initialized_ = true;
}

auto TimeSystem::Shutdown() -> void {
    domains_.clear();
    master_.reset();
    last_frame_ = FrameTick{};
    initialized_ = false;
}

auto TimeSystem::IsInitialized() const -> bool {
    return initialized_;
}

auto TimeSystem::Master() -> MasterClock& {
    return *master_;
}

auto TimeSystem::Master() const -> const MasterClock& {
    return *master_;
}

auto TimeSystem::CreateDomain(const std::string_view name, const ClockDesc& desc) -> DomainClock& {
    const auto key = std::string{name};
    auto [it, inserted] = domains_.try_emplace(key, desc);
    if (!inserted)
        it->second = DomainClock{desc};
    return it->second;
}

auto TimeSystem::FindDomain(const std::string_view name) -> DomainClock* {
    const auto it = domains_.find(std::string{name});
    return it == domains_.end() ? nullptr : &it->second;
}

auto TimeSystem::FindDomain(const std::string_view name) const -> const DomainClock* {
    const auto it = domains_.find(std::string{name});
    return it == domains_.end() ? nullptr : &it->second;
}

auto TimeSystem::Tick() -> FrameTick {
    if (!master_) {
        last_frame_ = FrameTick{};
        return last_frame_;
    }

    const auto host_delta = master_->ObserveHost();
    for (auto& entry : domains_)
        entry.second.Advance(host_delta);

    last_frame_ = FrameTick{master_->Now(), host_delta};
    return last_frame_;
}

auto TimeSystem::LastFrame() const -> const FrameTick& {
    return last_frame_;
}

auto TimeSystem::CreateDefaultDomains(const DefaultDomainConfig& config) -> void {
    ClockDesc game{};
    game.fixed_step_ns = config.game_fixed_step;
    game.max_steps_per_advance = config.max_steps_per_advance;

    ClockDesc physics{};
    physics.fixed_step_ns = config.physics_fixed_step;
    physics.max_steps_per_advance = config.max_steps_per_advance;

    ClockDesc render{};
    ClockDesc ui{};
    ClockDesc audio{};

    CreateDomain(domain::kGame, game);
    CreateDomain(domain::kPhysics, physics);
    CreateDomain(domain::kRender, render);
    CreateDomain(domain::kUi, ui);
    CreateDomain(domain::kAudio, audio);
}

} // namespace atom::time
