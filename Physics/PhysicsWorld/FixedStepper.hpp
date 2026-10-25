// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file FixedStepper.hpp
 * @brief 固定时间步累加器：把真实帧耗时切分成若干固定步，保证模拟确定性。
 *
 * 后端无关（消费 IPhysicsWorld），任何 IPhysicsBackend 的世界都可复用。
 * 用法：
 *
 *   FixedStepper stepper{1.0f / 60.0f, 8, 3};
 *   stepper.Advance(*world, frame_dt); // 返回本帧执行的固定步数
 *
 * max_substeps 防止"死亡螺旋"：单帧耗时远超固定步时，丢弃多余时间
 * （等效于降速慢动作），而不是无限追赶导致帧率崩塌。
 */

#ifndef ATOM_PHYSICS_PHYSICS_WORLD_FIXED_STEPPER_HPP
#define ATOM_PHYSICS_PHYSICS_WORLD_FIXED_STEPPER_HPP

#include <algorithm>

#include <Physics/Contracts/IPhysicsWorld.hpp>

namespace atom::physics {

class FixedStepper final {
    public:
        // fixed_dt 会钳到 [1e-4, 1.0]；默认 60 Hz + 8 速度迭代 + 3 位置迭代。
        explicit FixedStepper(const float fixed_dt = 1.0f / 60.0f, const int velocity_iterations = 8,
                              const int position_iterations = 3, const int max_substeps = 5)
            : fixed_dt_(std::clamp(fixed_dt, 1.0e-4f, 1.0f)),
              velocity_iterations_(velocity_iterations),
              position_iterations_(position_iterations),
              max_substeps_(std::max(1, max_substeps)) {}

        // 推进累加器并对 world 执行若干固定步。返回本帧执行的步数。
        [[nodiscard]] auto Advance(IPhysicsWorld& world, const float real_delta_time) -> int {
            accumulator_ += real_delta_time;

            int steps = 0;
            while (accumulator_ >= fixed_dt_ && steps < max_substeps_) {
                world.Step(fixed_dt_, velocity_iterations_, position_iterations_);
                accumulator_ -= fixed_dt_;
                ++steps;
            }

            // 到达上限：丢弃多余时间，保留 <1 帧的余量，避免死亡螺旋。
            if (steps == max_substeps_) {
                accumulator_ = std::min(accumulator_, fixed_dt_);
            }
            return steps;
        }

        auto Reset() -> void {
            accumulator_ = 0.0f;
        }

        [[nodiscard]] auto FixedDeltaTime() const -> float {
            return fixed_dt_;
        }
        [[nodiscard]] auto Accumulator() const -> float {
            return accumulator_;
        }

    private:
        float fixed_dt_;
        int velocity_iterations_;
        int position_iterations_;
        int max_substeps_;
        float accumulator_ = 0.0f;
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_PHYSICS_WORLD_FIXED_STEPPER_HPP
