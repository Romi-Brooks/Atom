/**
  * @file           : RenderGraph.hpp
  * @author         : Romi Brooks
  * @brief          : Backend-neutral render-pass dependency scheduler.
  * @attention      : The current implementation is a compatibility bridge for pass ordering.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_RENDER_CORE_RENDERGRAPH_HPP
#define ATOM_RENDER_CORE_RENDERGRAPH_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace atom::render {

using RenderPassId = uint32_t;
inline constexpr RenderPassId kInvalidRenderPass = 0;

struct RenderPassDesc {
    std::string name{};
    std::vector<RenderPassId> dependencies{};
};

// Small backend-neutral pass scheduler. It deliberately does not know about
// SDL_GPU/Vulkan resources yet; callbacks are the compatibility bridge while
// RenderDevice/CommandEncoder are migrated to the graph.
class RenderGraph final {
    public:
        using ExecuteFunction = std::function<bool()>;

        RenderGraph() = default;
        ~RenderGraph() = default;
        RenderGraph(const RenderGraph&) = delete;
        auto operator=(const RenderGraph&) -> RenderGraph& = delete;

        [[nodiscard]] auto AddPass(RenderPassDesc desc, ExecuteFunction execute) -> RenderPassId;
        [[nodiscard]] auto Execute() -> bool;
        auto Reset() -> void;
        [[nodiscard]] auto GetPassCount() const -> std::size_t;

    private:
        struct Pass {
            RenderPassDesc desc{};
            ExecuteFunction execute{};
        };

        std::vector<Pass> passes_{};
        bool executing_ = false;
};

} // namespace atom::render

#endif // ATOM_RENDER_CORE_RENDERGRAPH_HPP
