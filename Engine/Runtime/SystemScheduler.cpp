#include "SystemScheduler.hpp"

#include <algorithm>
#include <utility>

#include <Runtime/GameWorld.hpp>

namespace atom::runtime {

auto SystemScheduler::Add(const SystemPhase phase, SystemDescriptor descriptor) -> Result<void> {
    if (running_)
        return std::unexpected<Error>{MakeError(ErrorCode::InvalidArgument, Module::Core, "SystemScheduler::Add")
                                          .WithDetail("systems cannot be registered while a phase is running")};
    if (descriptor.name.empty())
        return std::unexpected<Error>{MakeError(ErrorCode::InvalidArgument, Module::Core, "SystemScheduler::Add")
                                          .WithDetail("system name must not be empty")};
    if (!descriptor.callback)
        return std::unexpected<Error>{MakeError(ErrorCode::InvalidArgument, Module::Core, "SystemScheduler::Add")
                                          .WithDetail("system callback must not be empty")};

    auto& systems = systems_[ToIndex(phase)];
    const auto duplicate = std::ranges::find(systems, descriptor.name, &SystemDescriptor::name);
    if (duplicate != systems.end())
        return std::unexpected<Error>{MakeError(ErrorCode::InvalidArgument, Module::Core, "SystemScheduler::Add")
                                          .WithDetail("system names must be unique within a phase")};

    systems.push_back(std::move(descriptor));
    return Ok();
}

auto SystemScheduler::Run(GameWorld& world, const FrameContext& context) -> Result<void> {
    if (running_)
        return std::unexpected<Error>{MakeError(ErrorCode::InvalidArgument, Module::Core, "SystemScheduler::Run")
                                          .WithDetail("nested system scheduling is not supported")};

    auto& systems = systems_[ToIndex(context.phase)];
    running_ = true;
    struct RunningGuard final {
            bool& running;
            ~RunningGuard() {
                running = false;
            }
    } guard{running_};

    for (const auto& system : systems) {
        if (auto result = system.callback(world, context); !result)
            return std::unexpected<Error>{std::move(result.error()).WithOperation("SystemScheduler::Run")};
    }
    return Ok();
}

auto SystemScheduler::Clear() -> void {
    if (running_)
        return;
    for (auto& phase : systems_)
        phase.clear();
}

auto SystemScheduler::Count(const SystemPhase phase) const noexcept -> std::size_t {
    return systems_[ToIndex(phase)].size();
}

} // namespace atom::runtime
