/**
  * @file           : LayoutTree.hpp
  * @author         : Romi Brooks
  * @brief          : Owning Yoga layout tree facade with stable node identifiers.
  * @attention      : Keeps low-level LayoutNode available for custom measure callbacks.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#ifndef ATOM_LAYOUT_LAYOUTTREE_HPP
#define ATOM_LAYOUT_LAYOUTTREE_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <Layout/LayoutConfig.hpp>
#include <Layout/LayoutNode.hpp>
#include <Layout/LayoutTypes.hpp>

namespace atom::layout {

// Owning convenience facade for a complete Yoga tree. LayoutNode remains
// available for low-level integrations; LayoutTree is the recommended API for
// UI/game code because node lifetime and parent bookkeeping are centralized.
class LayoutTree final {
    public:
        using NodeId = uint32_t;
        static constexpr NodeId kInvalidNode = 0;

        explicit LayoutTree(bool use_web_defaults = true);
        ~LayoutTree() = default;

        LayoutTree(const LayoutTree&) = delete;
        auto operator=(const LayoutTree&) -> LayoutTree& = delete;
        LayoutTree(LayoutTree&&) noexcept = default;
        auto operator=(LayoutTree&&) noexcept -> LayoutTree& = default;

        [[nodiscard]] auto Root() const -> NodeId { return root_id_; }
        auto SetPointScaleFactor(float scale_factor) -> void;
        [[nodiscard]] auto GetPointScaleFactor() const -> float;
        [[nodiscard]] auto CreateNode() -> NodeId;
        auto DestroyNode(NodeId id) -> bool;

        [[nodiscard]] auto GetNode(NodeId id) -> LayoutNode*;
        [[nodiscard]] auto GetNode(NodeId id) const -> const LayoutNode*;

        auto SetStyle(NodeId id, const LayoutStyle& style) -> bool;
        auto Append(NodeId parent, NodeId child) -> bool;
        auto Insert(NodeId parent, NodeId child, std::size_t index) -> bool;
        auto Remove(NodeId parent, NodeId child) -> bool;

        auto Calculate(std::optional<float> available_width = std::nullopt,
                       std::optional<float> available_height = std::nullopt,
                       Direction owner_direction = Direction::LeftToRight) -> bool;
        [[nodiscard]] auto GetLayout(NodeId id) const -> std::optional<Rect>;

    private:
        [[nodiscard]] auto IsValid(NodeId id) const -> bool;
        [[nodiscard]] auto IsDescendant(NodeId ancestor, NodeId candidate) const -> bool;

        LayoutConfig config_;
        std::vector<std::unique_ptr<LayoutNode>> nodes_{};
        std::vector<NodeId> parents_{};
        NodeId root_id_ = kInvalidNode;
};

} // namespace atom::layout

#endif // ATOM_LAYOUT_LAYOUTTREE_HPP
