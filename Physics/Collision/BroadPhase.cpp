// Copyright (c) 2026 Romi Brooks
// SPDX-License-Identifier: MIT

#include <Physics/Collision/BroadPhase.hpp>

#include <algorithm>
#include <cmath>

namespace atom::physics {

UniformGrid::UniformGrid(const float cell_size) : cell_size_(cell_size > 0.0f ? cell_size : 1.0f) {}

auto UniformGrid::EncodeCell(const int cell_x, const int cell_y) -> CellKey {
    // 两个 32 位有符号整数打包进 64 位（cell 坐标通常远小于 2^31）。
    return (static_cast<CellKey>(cell_x) << 32) ^ static_cast<std::uint32_t>(cell_y);
}

auto UniformGrid::EncodePair(const int a, const int b) -> CellKey {
    const int lo = std::min(a, b);
    const int hi = std::max(a, b);
    return (static_cast<CellKey>(lo) << 32) ^ static_cast<std::uint32_t>(hi);
}

auto UniformGrid::Clear() -> void {
    cells_.clear();
}

auto UniformGrid::Insert(const int proxy_id, const algo::Aabb2& aabb) -> void {
    const int min_x = static_cast<int>(std::floor(aabb.min.GetX() / cell_size_));
    const int max_x = static_cast<int>(std::floor(aabb.max.GetX() / cell_size_));
    const int min_y = static_cast<int>(std::floor(aabb.min.GetY() / cell_size_));
    const int max_y = static_cast<int>(std::floor(aabb.max.GetY() / cell_size_));
    for (int cx = min_x; cx <= max_x; ++cx) {
        for (int cy = min_y; cy <= max_y; ++cy) {
            cells_[EncodeCell(cx, cy)].push_back(proxy_id);
        }
    }
}

auto UniformGrid::FindPairs(const std::function<void(int, int)>& callback) const -> void {
    std::unordered_set<CellKey> seen{};
    for (const auto& [cell, proxies] : cells_) {
        (void)cell;
        for (std::size_t i = 0; i < proxies.size(); ++i) {
            for (std::size_t j = i + 1; j < proxies.size(); ++j) {
                const int a = proxies[i];
                const int b = proxies[j];
                if (seen.insert(EncodePair(a, b)).second) {
                    callback(a, b);
                }
            }
        }
    }
}

} // namespace atom::physics
