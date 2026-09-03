/**
  * @file           : RenderGraph.cpp
  * @author         : Romi Brooks
  * @brief          : RenderGraph dependency validation and execution.
  * @attention      : GPU resource ownership remains in the active backend during this migration phase.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "RenderGraph.hpp"

#include <algorithm>
#include <queue>
#include <utility>

namespace atom::render {

auto RenderGraph::AddPass(RenderPassDesc desc, ExecuteFunction execute) -> RenderPassId {
    if (!execute || executing_)
        return kInvalidRenderPass;
    passes_.push_back(Pass{std::move(desc), std::move(execute)});
    return static_cast<RenderPassId>(passes_.size());
}

auto RenderGraph::Execute() -> bool {
    if (executing_)
        return false;
    executing_ = true;

    const auto count = passes_.size();
    std::vector<uint32_t> indegree(count, 0);
    std::vector<std::vector<std::size_t>> outgoing(count);
    for (std::size_t index = 0; index < count; ++index) {
        for (const auto dependency : passes_[index].desc.dependencies) {
            if (dependency == kInvalidRenderPass || dependency > count) {
                executing_ = false;
                return false;
            }
            ++indegree[index];
            outgoing[dependency - 1].push_back(index);
        }
    }

    std::queue<std::size_t> ready;
    for (std::size_t index = 0; index < count; ++index) {
        if (indegree[index] == 0)
            ready.push(index);
    }

    std::size_t visited = 0;
    bool success = true;
    while (!ready.empty()) {
        const auto index = ready.front();
        ready.pop();
        ++visited;
        if (!passes_[index].execute())
            success = false;
        for (const auto next : outgoing[index]) {
            if (--indegree[next] == 0)
                ready.push(next);
        }
    }
    executing_ = false;
    return visited == count && success;
}

auto RenderGraph::Reset() -> void {
    if (!executing_)
        passes_.clear();
}

auto RenderGraph::GetPassCount() const -> std::size_t {
    return passes_.size();
}

} // namespace atom::render
