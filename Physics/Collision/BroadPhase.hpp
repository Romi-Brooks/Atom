// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

/**
 * @file BroadPhase.hpp
 * @brief 宽相位：给定一组 AABB 代理，输出潜在重叠对。
 *
 * 接口（IBroadPhase）与实现（UniformGrid）分离：后续换成动态 AABB 树（大量
 * 静态体 / 不均匀分布）时接口不变，窄相位只消费它产出的候选对。
 */

#ifndef ATOM_PHYSICS_COLLISION_BROAD_PHASE_HPP
#define ATOM_PHYSICS_COLLISION_BROAD_PHASE_HPP

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Algorithm/Geometry/Primitives.hpp>

namespace atom::physics {

class IBroadPhase {
    public:
        virtual ~IBroadPhase() = default;
        virtual auto Clear() -> void = 0;
        virtual auto Insert(int proxy_id, const algo::Aabb2& aabb) -> void = 0;
        // 输出每个潜在重叠对（proxy_id, proxy_id），不保证顺序、不保证去重。
        virtual auto FindPairs(const std::function<void(int, int)>& callback) const -> void = 0;
};

// 均匀空间网格。cell_size 应接近物体平均尺寸。M2 默认实现。
class UniformGrid final : public IBroadPhase {
    public:
        explicit UniformGrid(float cell_size);

        auto Clear() -> void override;
        auto Insert(int proxy_id, const algo::Aabb2& aabb) -> void override;
        auto FindPairs(const std::function<void(int, int)>& callback) const -> void override;

    private:
        using CellKey = std::int64_t;

        static auto EncodeCell(int cell_x, int cell_y) -> CellKey;
        static auto EncodePair(int a, int b) -> CellKey;

        float cell_size_ = 1.0f;
        std::unordered_map<CellKey, std::vector<int>> cells_{};
};

} // namespace atom::physics

#endif // ATOM_PHYSICS_COLLISION_BROAD_PHASE_HPP
