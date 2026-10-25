#include "EntityCommandBuffer.hpp"

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

namespace atom::runtime {

auto EntityCommandBuffer::QueueCreate() -> void {
    commands_.emplace_back(CreateCommand{});
}

auto EntityCommandBuffer::QueueCreate(SpawnSetup setup) -> void {
    commands_.emplace_back(CreateCommand{std::move(setup)});
}

auto EntityCommandBuffer::QueueDestroy(const ecs::Entity entity) -> void {
    commands_.emplace_back(DestroyCommand{entity});
}

auto EntityCommandBuffer::Flush(ecs::EntityWorld& world) -> Result<void> {
    auto pending = std::move(commands_);
    commands_.clear();

    std::optional<Error> first_error;
    for (auto& command : pending) {
        const auto result = std::visit(
            [&world](auto& typed_command) -> Result<void> {
                using CommandType = std::decay_t<decltype(typed_command)>;
                if constexpr (std::same_as<CommandType, CreateCommand>) {
                    const auto entity = world.CreateEntity();
                    if (!typed_command.setup)
                        return Ok();
                    if (auto setup = typed_command.setup(world, entity); !setup) {
                        const auto cleanup = world.Destroy(entity);
                        (void)cleanup;
                        return std::unexpected<Error>{
                            std::move(setup.error()).WithOperation("EntityCommandBuffer::Flush(Create)")};
                    }
                    return Ok();
                } else {
                    if (auto destroy = world.Destroy(typed_command.entity); !destroy)
                        return std::unexpected<Error>{
                            std::move(destroy.error()).WithOperation("EntityCommandBuffer::Flush(Destroy)")};
                    return Ok();
                }
            },
            command);
        if (!result && !first_error)
            first_error.emplace(std::move(result.error()));
    }

    if (first_error)
        return std::unexpected<Error>{std::move(*first_error)};
    return Ok();
}

auto EntityCommandBuffer::Clear() -> void {
    commands_.clear();
}

} // namespace atom::runtime
