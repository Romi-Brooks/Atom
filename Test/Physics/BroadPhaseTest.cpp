#include <Algorithm/Geometry/Primitives.hpp>
#include <Physics/Collision/BroadPhase.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <utility>
#include <vector>

auto main() -> int {
    using namespace atom::physics;
    using namespace atom::algo;

    const auto aabb = [](const float x, const float y, const float w, const float h) {
        return Aabb2{Vec2{x, y}, Vec2{x + w, y + h}};
    };

    // 重叠对 → 1 对
    {
        UniformGrid grid{1.0f};
        grid.Insert(0, aabb(0.0f, 0.0f, 1.0f, 1.0f));
        grid.Insert(1, aabb(0.5f, 0.5f, 1.0f, 1.0f));
        int pairs = 0;
        grid.FindPairs([&](int, int) { ++pairs; });
        ATOM_CHECK(pairs == 1);
    }

    // 分离 → 0 对
    {
        UniformGrid grid{1.0f};
        grid.Insert(0, aabb(0.0f, 0.0f, 1.0f, 1.0f));
        grid.Insert(1, aabb(5.0f, 5.0f, 1.0f, 1.0f));
        int pairs = 0;
        grid.FindPairs([&](int, int) { ++pairs; });
        ATOM_CHECK(pairs == 0);
    }

    // 三个 body：两个重叠 + 一个分离 → 恰好 1 对
    {
        UniformGrid grid{1.0f};
        grid.Insert(0, aabb(0.0f, 0.0f, 1.0f, 1.0f));
        grid.Insert(1, aabb(0.5f, 0.0f, 1.0f, 1.0f));
        grid.Insert(2, aabb(10.0f, 10.0f, 1.0f, 1.0f));
        int pairs = 0;
        grid.FindPairs([&](int, int) { ++pairs; });
        ATOM_CHECK(pairs == 1);
    }

    // 跨多个 cell 的大 AABB 也能找到 pair
    {
        UniformGrid grid{1.0f};
        grid.Insert(0, aabb(0.0f, 0.0f, 3.0f, 1.0f));
        grid.Insert(1, aabb(2.5f, 0.0f, 1.0f, 1.0f));
        int pairs = 0;
        grid.FindPairs([&](int, int) { ++pairs; });
        ATOM_CHECK(pairs == 1);
    }

    // 同一 pair 跨多个 cell 不重复输出
    {
        UniformGrid grid{1.0f};
        grid.Insert(0, aabb(0.0f, 0.0f, 3.0f, 3.0f));
        grid.Insert(1, aabb(0.5f, 0.5f, 1.0f, 1.0f));
        int pairs = 0;
        grid.FindPairs([&](int, int) { ++pairs; });
        ATOM_CHECK(pairs == 1);
    }

    // unordered_map 的 cell 遍历不得泄漏到调用方：候选对按稳定索引顺序输出。
    {
        UniformGrid grid{1.0f};
        grid.Insert(2, aabb(0.0f, 0.0f, 1.0f, 1.0f));
        grid.Insert(0, aabb(0.1f, 0.0f, 1.0f, 1.0f));
        grid.Insert(1, aabb(0.2f, 0.0f, 1.0f, 1.0f));
        std::vector<std::pair<int, int>> pairs;
        grid.FindPairs([&](const int a, const int b) { pairs.emplace_back(a, b); });
        ATOM_CHECK(pairs == std::vector<std::pair<int, int>>{{0, 1}, {0, 2}, {1, 2}});
    }

    // Clear 后无 pair
    {
        UniformGrid grid{1.0f};
        grid.Insert(0, aabb(0.0f, 0.0f, 1.0f, 1.0f));
        grid.Insert(1, aabb(0.5f, 0.0f, 1.0f, 1.0f));
        grid.Clear();
        int pairs = 0;
        grid.FindPairs([&](int, int) { ++pairs; });
        ATOM_CHECK(pairs == 0);
    }

    return 0;
}
