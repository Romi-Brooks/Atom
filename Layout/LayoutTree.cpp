/**
  * @file           : LayoutTree.cpp
  * @author         : Romi Brooks
  * @brief          : LayoutTree ownership and Yoga node relationship implementation.
  * @attention      : Destruction detaches children before releasing Yoga nodes.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

#include "LayoutTree.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include <Log/LogSystem.hpp>

namespace atom::layout {

LayoutTree::LayoutTree(const bool use_web_defaults) : config_{use_web_defaults} {
    root_id_ = CreateNode();
}

auto LayoutTree::SetPointScaleFactor(const float scale_factor) -> void {
    config_.SetPointScaleFactor(scale_factor);
}

auto LayoutTree::GetPointScaleFactor() const -> float {
    return config_.GetPointScaleFactor();
}

auto LayoutTree::IsValid(const NodeId id) const -> bool {
    return id != kInvalidNode && id <= nodes_.size() && nodes_[id - 1] != nullptr;
}

auto LayoutTree::CreateNode() -> NodeId {
    nodes_.push_back(std::make_unique<LayoutNode>(config_));
    parents_.push_back(kInvalidNode);
    return static_cast<NodeId>(nodes_.size());
}

auto LayoutTree::GetNode(const NodeId id) -> LayoutNode* {
    return IsValid(id) ? nodes_[id - 1].get() : nullptr;
}

auto LayoutTree::GetNode(const NodeId id) const -> const LayoutNode* {
    return IsValid(id) ? nodes_[id - 1].get() : nullptr;
}

auto LayoutTree::IsDescendant(const NodeId ancestor, const NodeId candidate) const -> bool {
    for (auto current = parents_[candidate - 1]; current != kInvalidNode;) {
        if (current == ancestor)
            return true;
        current = parents_[current - 1];
    }
    return false;
}

auto LayoutTree::Append(const NodeId parent, const NodeId child) -> bool {
    return Insert(parent, child, GetNode(parent) ? GetNode(parent)->GetChildCount() : 0);
}

auto LayoutTree::Insert(const NodeId parent, const NodeId child, const std::size_t index) -> bool {
    if (!IsValid(parent) || !IsValid(child) || parent == child || IsDescendant(child, parent) ||
        parents_[child - 1] != kInvalidNode) {
        LOG_WARNING(LogChannel::CORE, "LayoutTree rejected an invalid parent/child relationship");
        return false;
    }
    try {
        nodes_[parent - 1]->InsertChild(*nodes_[child - 1], index);
    } catch (...) {
        return false;
    }
    parents_[child - 1] = parent;
    return true;
}

auto LayoutTree::Remove(const NodeId parent, const NodeId child) -> bool {
    if (!IsValid(parent) || !IsValid(child) || parents_[child - 1] != parent)
        return false;
    try {
        nodes_[parent - 1]->RemoveChild(*nodes_[child - 1]);
    } catch (...) {
        return false;
    }
    parents_[child - 1] = kInvalidNode;
    return true;
}

auto LayoutTree::DestroyNode(const NodeId id) -> bool {
    if (!IsValid(id) || id == root_id_)
        return false;

    if (parents_[id - 1] != kInvalidNode)
        Remove(parents_[id - 1], id);

    std::vector<NodeId> victims;
    victims.push_back(id);
    for (std::size_t i = 0; i < victims.size(); ++i) {
        const auto parent = victims[i];
        for (NodeId candidate = 1; candidate <= nodes_.size(); ++candidate) {
            if (IsValid(candidate) && parents_[candidate - 1] == parent)
                victims.push_back(candidate);
        }
    }

    // Destroy descendants first. LayoutNode's destructor also detaches as a
    // safety net for trees assembled through the low-level API.
    std::reverse(victims.begin(), victims.end());
    for (const auto victim : victims) {
        nodes_[victim - 1].reset();
        parents_[victim - 1] = kInvalidNode;
    }
    return true;
}

auto LayoutTree::SetStyle(const NodeId id, const LayoutStyle& style) -> bool {
    if (auto* node = GetNode(id); node != nullptr) {
        node->SetStyle(style);
        return true;
    }
    return false;
}

auto LayoutTree::Calculate(const std::optional<float> available_width,
                           const std::optional<float> available_height, const Direction owner_direction) -> bool {
    auto* root = GetNode(root_id_);
    if (root == nullptr)
        return false;
    root->CalculateLayout(available_width, available_height, owner_direction);
    return true;
}

auto LayoutTree::GetLayout(const NodeId id) const -> std::optional<Rect> {
    if (const auto* node = GetNode(id); node != nullptr)
        return node->GetLayout();
    return std::nullopt;
}

} // namespace atom::layout
