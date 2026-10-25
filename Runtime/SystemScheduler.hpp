/**
 * @file SystemScheduler.hpp
 * @brief Fixed-order scheduler for GameWorld systems.
 */
#ifndef ATOM_RUNTIME_SYSTEMSCHEDULER_HPP
#define ATOM_RUNTIME_SYSTEMSCHEDULER_HPP

#include <array>
#include <cstddef>
#include <vector>

#include <Runtime/System.hpp>

namespace atom::runtime {

class SystemScheduler final {
    public:
        [[nodiscard]] auto Add(SystemPhase phase, SystemDescriptor descriptor) -> Result<void>;
        [[nodiscard]] auto Run(GameWorld& world, const FrameContext& context) -> Result<void>;

        auto Clear() -> void;
        [[nodiscard]] auto Count(SystemPhase phase) const noexcept -> std::size_t;
        [[nodiscard]] auto IsRunning() const noexcept -> bool {
            return running_;
        }

    private:
        [[nodiscard]] static constexpr auto ToIndex(const SystemPhase phase) noexcept -> std::size_t {
            return static_cast<std::size_t>(phase);
        }

        static constexpr std::size_t phase_count_ = static_cast<std::size_t>(SystemPhase::LateUpdate) + 1;
        std::array<std::vector<SystemDescriptor>, phase_count_> systems_{};
        bool running_ = false;
};

} // namespace atom::runtime

#endif // ATOM_RUNTIME_SYSTEMSCHEDULER_HPP
