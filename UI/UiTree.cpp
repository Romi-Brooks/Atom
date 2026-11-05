#include "UiTree.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <Input/InputSystem.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>
#include <Render/Text/Font.hpp>

namespace atom::ui {
namespace {

[[nodiscard]] auto ToRect(const layout::Rect& rect) -> algo::Rect {
    return {rect.left, rect.top, rect.width, rect.height};
}

[[nodiscard]] auto IsTransparent(const render::Color color) -> bool {
    return color.a == 0;
}

} // namespace

UiTree::UiTree() : root_(layout_tree_.Root()) {
    nodes_.resize(root_);
    Node root{};
    root.kind = WidgetKind::Panel;
    root.pointer_events = false;
    root.layout.flex_direction = layout::FlexDirection::Column;
    nodes_[root_ - 1] = std::move(root);
    static_cast<void>(layout_tree_.SetStyle(root_, nodes_[root_ - 1]->layout));
}

UiTree::~UiTree() { ReleaseInputCapture(); }

auto UiTree::GetNode(const NodeId id) -> Node* {
    if (id == invalid_node || id > nodes_.size() || !nodes_[id - 1])
        return nullptr;
    return &*nodes_[id - 1];
}

auto UiTree::GetNode(const NodeId id) const -> const Node* {
    if (id == invalid_node || id > nodes_.size() || !nodes_[id - 1])
        return nullptr;
    return &*nodes_[id - 1];
}

auto UiTree::CreateNode(const NodeId parent, const WidgetKind kind, const layout::LayoutStyle& layout,
                        const VisualStyle& visual, std::string text, const bool pointer_events,
                        ClickHandler on_click) -> NodeId {
    if (!GetNode(parent))
        return invalid_node;

    const auto id = layout_tree_.CreateNode();
    if (id != nodes_.size() + 1)
        return invalid_node; // LayoutTree IDs are append-only; keep both trees aligned.

    Node node{};
    node.kind = kind;
    node.parent = parent;
    node.layout = layout;
    node.visual = visual;
    node.text = std::move(text);
    node.pointer_events = pointer_events;
    node.on_click = std::move(on_click);
    nodes_.emplace_back(std::move(node));
    if (!layout_tree_.SetStyle(id, layout) || !layout_tree_.Append(parent, id)) {
        nodes_.back().reset();
        static_cast<void>(layout_tree_.DestroyNode(id));
        return invalid_node;
    }
    GetNode(parent)->children.push_back(id);
    return id;
}

auto UiTree::CreatePanel(const NodeId parent, const layout::LayoutStyle& layout, const VisualStyle& visual) -> NodeId {
    return CreateNode(parent, WidgetKind::Panel, layout, visual, {}, false, {});
}

auto UiTree::CreateLabel(const NodeId parent, std::string text, const layout::LayoutStyle& layout,
                         const VisualStyle& visual) -> NodeId {
    return CreateNode(parent, WidgetKind::Label, layout, visual, std::move(text), false, {});
}

auto UiTree::CreateButton(const NodeId parent, std::string text, const layout::LayoutStyle& layout,
                          const VisualStyle& visual, ClickHandler on_click) -> NodeId {
    return CreateNode(parent, WidgetKind::Button, layout, visual, std::move(text), true, std::move(on_click));
}

auto UiTree::DestroySubtree(const NodeId id) -> void {
    auto* node = GetNode(id);
    if (!node)
        return;
    const auto children = node->children;
    for (const auto child : children)
        DestroySubtree(child);
    nodes_[id - 1].reset();
    if (hovered_ == id)
        hovered_ = invalid_node;
    if (pressed_ == id)
        pressed_ = invalid_node;
    if (pointer_capture_ == id)
        pointer_capture_ = invalid_node;
    if (focused_ == id)
        focused_ = invalid_node;
}

auto UiTree::DestroyNode(const NodeId id) -> bool {
    if (id == root_ || !GetNode(id))
        return false;
    auto* node = GetNode(id);
    const auto parent = node->parent;
    if (auto* parent_node = GetNode(parent); parent_node) {
        std::erase(parent_node->children, id);
    }
    DestroySubtree(id);
    return layout_tree_.DestroyNode(id);
}

auto UiTree::SetLayoutStyle(const NodeId id, const layout::LayoutStyle& style) -> bool {
    auto* node = GetNode(id);
    if (!node)
        return false;
    node->layout = style;
    return layout_tree_.SetStyle(id, style);
}

auto UiTree::SetVisualStyle(const NodeId id, const VisualStyle& style) -> bool {
    if (auto* node = GetNode(id); node) {
        node->visual = style;
        return true;
    }
    return false;
}

auto UiTree::SetText(const NodeId id, std::string text) -> bool {
    if (auto* node = GetNode(id); node) {
        node->text = std::move(text);
        return true;
    }
    return false;
}

auto UiTree::SetOnClick(const NodeId id, ClickHandler on_click) -> bool {
    if (auto* node = GetNode(id); node && node->kind == WidgetKind::Button) {
        node->on_click = std::move(on_click);
        return true;
    }
    return false;
}

auto UiTree::SetVisible(const NodeId id, const bool visible) -> bool {
    if (auto* node = GetNode(id); node) {
        node->visible = visible;
        return true;
    }
    return false;
}

auto UiTree::SetEnabled(const NodeId id, const bool enabled) -> bool {
    if (auto* node = GetNode(id); node) {
        node->enabled = enabled;
        if (!enabled && (focused_ == id || pressed_ == id || pointer_capture_ == id)) {
            if (focused_ == id)
                focused_ = invalid_node;
            if (pressed_ == id)
                pressed_ = invalid_node;
            if (pointer_capture_ == id)
                pointer_capture_ = invalid_node;
        }
        return true;
    }
    return false;
}

auto UiTree::Layout(const float width, const float height) -> bool {
    if (!std::isfinite(width) || !std::isfinite(height) || width < 0.0f || height < 0.0f)
        return false;
    auto* root = GetNode(root_);
    if (!root)
        return false;
    auto root_style = root->layout;
    root_style.width = layout::Length::Points(width);
    root_style.height = layout::Length::Points(height);
    if (!layout_tree_.SetStyle(root_, root_style))
        return false;
    return layout_tree_.Calculate(width, height);
}

auto UiTree::GetBounds(const NodeId id) const -> std::optional<algo::Rect> {
    const auto* node = GetNode(id);
    if (!node)
        return std::nullopt;
    const auto layout = layout_tree_.GetLayout(id);
    if (!layout)
        return std::nullopt;

    // Yoga's layout left/top are relative to the immediate parent. Rendering,
    // clipping and window input are all in one screen-space canvas, so convert
    // the local Yoga box exactly once at this boundary. Passing local boxes to
    // Renderer2D made a nested button at (26, y, 408, h) draw near the window
    // origin; a clipped parent at (410, y) then exposed only its final ~24 px.
    auto bounds = ToRect(*layout);
    for (auto parent = node->parent; parent != invalid_node;) {
        const auto parent_layout = layout_tree_.GetLayout(parent);
        const auto* parent_node = GetNode(parent);
        if (!parent_layout || !parent_node)
            return std::nullopt;
        bounds.x += parent_layout->left;
        bounds.y += parent_layout->top;
        parent = parent_node->parent;
    }
    return bounds;
}

auto UiTree::HitTest(const NodeId id, const float x, const float y) const -> NodeId {
    const auto* node = GetNode(id);
    const auto bounds = GetBounds(id);
    if (!node || !bounds || !node->visible)
        return invalid_node;
    if (node->visual.clip_children && !bounds->Contains(x, y))
        return invalid_node;

    for (auto it = node->children.rbegin(); it != node->children.rend(); ++it) {
        if (const auto hit = HitTest(*it, x, y); hit != invalid_node)
            return hit;
    }
    return node->pointer_events && node->enabled && bounds->Contains(x, y) ? id : invalid_node;
}

auto UiTree::HitTest(const float x, const float y) const -> NodeId { return HitTest(root_, x, y); }

auto UiTree::ClaimMouseForFrame() -> void {
    input::InputSystem::GetInstance().SetUiCapture(input::Claim::Own, input::Claim::NoOpinion,
                                                   input::Claim::NoOpinion, "atom.ui");
}

auto UiTree::ClaimKeyboardForFrame() -> void {
    input::InputSystem::GetInstance().SetUiCapture(input::Claim::NoOpinion, input::Claim::Own,
                                                   input::Claim::NoOpinion, "atom.ui");
}

auto UiTree::InvokeClick(const NodeId id) -> void {
    const auto* node = GetNode(id);
    if (node && node->enabled && node->kind == WidgetKind::Button && node->on_click)
        node->on_click();
}

auto UiTree::HandleEvent(const window::IEvent& event) -> bool {
    switch (event.type) {
    case window::EventType::MouseMoved: {
        const auto& mouse = std::get<window::MouseEvent>(event.data);
        hovered_ = pointer_capture_ != invalid_node ? pointer_capture_ : HitTest(mouse.x, mouse.y);
        if (pointer_capture_ != invalid_node || modal_) {
            ClaimMouseForFrame();
            return true;
        }
        return false;
    }
    case window::EventType::MouseButtonPressed: {
        const auto& mouse = std::get<window::MouseEvent>(event.data);
        if (mouse.button != input::MouseButton::Left)
            return false;
        const auto hit = HitTest(mouse.x, mouse.y);
        hovered_ = hit;
        if (hit == invalid_node) {
            focused_ = invalid_node;
            if (modal_) {
                ClaimMouseForFrame();
                return true;
            }
            return false;
        }
        pressed_ = hit;
        pointer_capture_ = hit;
        focused_ = hit;
        ClaimMouseForFrame();
        return true;
    }
    case window::EventType::MouseButtonReleased: {
        const auto& mouse = std::get<window::MouseEvent>(event.data);
        if (mouse.button != input::MouseButton::Left || pointer_capture_ == invalid_node) {
            if (modal_) {
                ClaimMouseForFrame();
                return true;
            }
            return false;
        }
        const auto captured = pointer_capture_;
        const auto hit = HitTest(mouse.x, mouse.y);
        pointer_capture_ = invalid_node;
        pressed_ = invalid_node;
        hovered_ = hit;
        ClaimMouseForFrame();
        if (hit == captured)
            InvokeClick(captured);
        return true;
    }
    case window::EventType::KeyPressed: {
        const auto& key = std::get<window::KeyEvent>(event.data);
        if (focused_ == invalid_node || (key.key != input::Key::Enter && key.key != input::Key::Space)) {
            if (modal_) {
                ClaimKeyboardForFrame();
                return true;
            }
            return false;
        }
        const auto* focused = GetNode(focused_);
        if (!focused || !focused->enabled || focused->kind != WidgetKind::Button) {
            if (modal_) {
                ClaimKeyboardForFrame();
                return true;
            }
            return false;
        }
        ClaimKeyboardForFrame();
        InvokeClick(focused_);
        return true;
    }
    case window::EventType::KeyReleased:
    case window::EventType::MouseWheel:
        if (modal_) {
            if (event.type == window::EventType::KeyReleased)
                ClaimKeyboardForFrame();
            else
                ClaimMouseForFrame();
            return true;
        }
        return false;
    case window::EventType::FocusChanged: {
        const auto& focus = std::get<window::FocusEvent>(event.data);
        if (!focus.focused) {
            hovered_ = invalid_node;
            pressed_ = invalid_node;
            pointer_capture_ = invalid_node;
            focused_ = invalid_node;
            ReleaseInputCapture();
        }
        return false;
    }
    default:
        return false;
    }
}

auto UiTree::Update() -> void {
    const auto mouse_claim = pointer_capture_ != invalid_node ? input::Claim::Own : input::Claim::Release;
    const auto keyboard_claim = input::Claim::Release;
    input::InputSystem::GetInstance().SetUiCapture(mouse_claim, keyboard_claim, input::Claim::Release, "atom.ui");
}

auto UiTree::ReleaseInputCapture() -> void {
    input::InputSystem::GetInstance().SetUiCapture(input::Claim::Release, input::Claim::Release,
                                                   input::Claim::Release, "atom.ui");
}

auto UiTree::DrawNode(const NodeId id, render::Renderer2D& renderer, const render::Font* font) const -> void {
    const auto* node = GetNode(id);
    const auto bounds = GetBounds(id);
    if (!node || !bounds || !node->visible)
        return;

    auto background = node->visual.background;
    if (node->kind == WidgetKind::Button) {
        if (!node->enabled && !IsTransparent(node->visual.disabled_background))
            background = node->visual.disabled_background;
        else if (pressed_ == id && !IsTransparent(node->visual.pressed_background))
            background = node->visual.pressed_background;
        else if (hovered_ == id && !IsTransparent(node->visual.hover_background))
            background = node->visual.hover_background;
    }
    if (node->visual.draw_background && !IsTransparent(background))
        renderer.DrawRect(*bounds, background);

    if ((node->kind == WidgetKind::Label || node->kind == WidgetKind::Button) && font && !node->text.empty()) {
        const float x = bounds->x + node->visual.text_padding;
        const float y = bounds->y + std::max(0.0f, (bounds->height - node->visual.text_size) * 0.5f);
        const float width = std::max(0.0f, bounds->width - node->visual.text_padding * 2.0f);
        renderer.DrawText(*font, node->text, x, y, node->visual.text, node->visual.text_size, width);
    }

    if (node->visual.clip_children)
        renderer.PushClip(*bounds);
    for (const auto child : node->children)
        DrawNode(child, renderer, font);
    if (node->visual.clip_children)
        renderer.PopClip();
}

auto UiTree::Render(render::Renderer2D& renderer, const render::Font* font) const -> void { DrawNode(root_, renderer, font); }

} // namespace atom::ui
