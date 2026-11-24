/**
 * @file UiTree.hpp
 * @brief Retained game-UI tree: layout, hit testing, focus, and P0 widgets.
 */
#ifndef ATOM_UI_UI_TREE_HPP
#define ATOM_UI_UI_TREE_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <Algorithm/Geometry/Rect.hpp>
#include <Backend/Contracts/Render/IRenderTypes.hpp>
#include <Backend/Contracts/Window/IWindow.hpp>
#include <Layout/LayoutTree.hpp>

namespace atom::render {
class Font;
class Renderer2D;
}

namespace atom::ui {

using NodeId = layout::LayoutTree::NodeId;
inline constexpr NodeId invalid_node = layout::LayoutTree::invalid_node;

enum class WidgetKind : std::uint8_t { Panel, Label, Button };

// P0 keeps visual styling deliberately small. It is a value copied into the
// retained tree, not a renderer object, so themes can later replace it without
// coupling widget lifetime to GPU resources.
struct VisualStyle final {
        render::Color background{0, 0, 0, 0};
        render::Color hover_background{0, 0, 0, 0};
        render::Color pressed_background{0, 0, 0, 0};
        render::Color disabled_background{0, 0, 0, 0};
        render::Color text{255, 255, 255, 255};
        float text_size = 18.0f;
        float text_padding = 12.0f;
        bool draw_background = false;
        bool clip_children = false;
};

using ClickHandler = std::function<void()>;

// UI is intentionally a separate retained tree, rather than ECS entities:
// widget lifetime, hit-test order, focus and Yoga layout are local to a page.
// A Screen (or another game-owned presentation object) owns one UiTree and
// calls Layout/HandleEvent/Update/Render in its normal lifecycle.
class UiTree final {
    public:
        UiTree();
        ~UiTree();
        UiTree(const UiTree&) = delete;
        auto operator=(const UiTree&) -> UiTree& = delete;
        UiTree(UiTree&&) = delete;
        auto operator=(UiTree&&) -> UiTree& = delete;

        [[nodiscard]] auto Root() const noexcept -> NodeId { return root_; }

        [[nodiscard]] auto CreatePanel(NodeId parent, const layout::LayoutStyle& layout = {},
                                       const VisualStyle& visual = {}) -> NodeId;
        [[nodiscard]] auto CreateLabel(NodeId parent, std::string text, const layout::LayoutStyle& layout = {},
                                       const VisualStyle& visual = {}) -> NodeId;
        [[nodiscard]] auto CreateButton(NodeId parent, std::string text, const layout::LayoutStyle& layout = {},
                                        const VisualStyle& visual = {}, ClickHandler on_click = {}) -> NodeId;
        auto DestroyNode(NodeId id) -> bool;

        auto SetLayoutStyle(NodeId id, const layout::LayoutStyle& style) -> bool;
        auto SetVisualStyle(NodeId id, const VisualStyle& style) -> bool;
        auto SetText(NodeId id, std::string text) -> bool;
        auto SetOnClick(NodeId id, ClickHandler on_click) -> bool;
        auto SetVisible(NodeId id, bool visible) -> bool;
        auto SetEnabled(NodeId id, bool enabled) -> bool;
        // A modal tree consumes pointer/keyboard events that no child handled.
        // This is how a pause dialog blocks the game screen below it without
        // making a transparent full-window button part of every UI hierarchy.
        auto SetModal(bool modal) noexcept -> void { modal_ = modal; }
        [[nodiscard]] auto IsModal() const noexcept -> bool { return modal_; }

        // `width` and `height` are physical output pixels. UI canvas space is
        // deliberately screen space (top-left origin), independent of a world
        // camera used by another Renderer2D submission.
        auto Layout(float width, float height) -> bool;
        [[nodiscard]] auto GetBounds(NodeId id) const -> std::optional<algo::Rect>;
        [[nodiscard]] auto GetHoveredNode() const noexcept -> NodeId { return hovered_; }
        [[nodiscard]] auto GetFocusedNode() const noexcept -> NodeId { return focused_; }

        // Dispatches normalized window events, returning true only when the UI
        // consumed the event. It owns pointer capture from press through
        // release, and composes one aggregate game-UI claim into InputSystem.
        auto HandleEvent(const window::IEvent& event) -> bool;
        // Call during the owning page's Update(), after Runtime's fixed update.
        // It keeps persistent pointer capture claimed and releases transient
        // button/keyboard claims once that frame's game simulation is over.
        auto Update() -> void;
        auto ReleaseInputCapture() -> void;

        // Draws the retained tree into an active Renderer2D frame. `font` is
        // optional so a game can show non-text UI while its font asset loads.
        auto Render(render::Renderer2D& renderer, const render::Font* font = nullptr) const -> void;

    private:
        struct Node final {
                WidgetKind kind = WidgetKind::Panel;
                NodeId parent = invalid_node;
                std::vector<NodeId> children{};
                layout::LayoutStyle layout{};
                VisualStyle visual{};
                std::string text{};
                ClickHandler on_click{};
                bool visible = true;
                bool enabled = true;
                bool pointer_events = false;
        };

        [[nodiscard]] auto GetNode(NodeId id) -> Node*;
        [[nodiscard]] auto GetNode(NodeId id) const -> const Node*;
        [[nodiscard]] auto CreateNode(NodeId parent, WidgetKind kind, const layout::LayoutStyle& layout,
                                      const VisualStyle& visual, std::string text, bool pointer_events,
                                      ClickHandler on_click) -> NodeId;
        auto DestroySubtree(NodeId id) -> void;
        [[nodiscard]] auto HitTest(NodeId id, float x, float y) const -> NodeId;
        [[nodiscard]] auto HitTest(float x, float y) const -> NodeId;
        auto DrawNode(NodeId id, render::Renderer2D& renderer, const render::Font* font) const -> void;
        auto InvokeClick(NodeId id) -> void;
        auto ClaimMouseForFrame() -> void;
        auto ClaimKeyboardForFrame() -> void;

        layout::LayoutTree layout_tree_{};
        std::vector<std::optional<Node>> nodes_{};
        NodeId root_ = invalid_node;
        NodeId hovered_ = invalid_node;
        NodeId pressed_ = invalid_node;
        NodeId pointer_capture_ = invalid_node;
        NodeId focused_ = invalid_node;
        bool modal_ = false;
};

} // namespace atom::ui

#endif // ATOM_UI_UI_TREE_HPP
