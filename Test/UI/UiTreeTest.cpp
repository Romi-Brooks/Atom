#include <Input/InputSystem.hpp>
#include <Test/Support/TestHelpers.hpp>
#include <UI/UiTree.hpp>

namespace {

auto MouseEvent(const atom::window::EventType type, const float x, const float y,
                const atom::input::MouseButton button = atom::input::MouseButton::Left) -> atom::window::IEvent {
    atom::window::IEvent event{};
    event.type = type;
    event.data = atom::window::MouseEvent{x, y, button};
    return event;
}

auto ButtonStyle(const float x, const float y) -> atom::layout::LayoutStyle {
    atom::layout::LayoutStyle style{};
    style.position_type = atom::layout::PositionType::Absolute;
    style.position.left = atom::layout::Length::Points(x);
    style.position.top = atom::layout::Length::Points(y);
    style.width = atom::layout::Length::Points(160.0f);
    style.height = atom::layout::Length::Points(48.0f);
    return style;
}

} // namespace

auto main() -> int {
    atom::ui::UiTree tree{};
    int back_clicks = 0;
    int front_clicks = 0;

    const auto back = tree.CreateButton(tree.Root(), "back", ButtonStyle(20.0f, 20.0f), {}, [&] { ++back_clicks; });
    const auto front = tree.CreateButton(tree.Root(), "front", ButtonStyle(20.0f, 20.0f), {}, [&] { ++front_clicks; });
    ATOM_CHECK(back != atom::ui::invalid_node);
    ATOM_CHECK(front != atom::ui::invalid_node);
    ATOM_CHECK(tree.Layout(800.0f, 600.0f));

    const auto bounds = tree.GetBounds(front);
    ATOM_CHECK(bounds.has_value());
    ATOM_CHECK(bounds->x == 20.0f);
    ATOM_CHECK(bounds->y == 20.0f);
    ATOM_CHECK(bounds->width == 160.0f);
    ATOM_CHECK(bounds->height == 48.0f);

    // LayoutTree exposes local Yoga boxes. UiTree's public bounds must instead
    // be canvas coordinates, otherwise a nested child is rendered/input-tested
    // near (0,0) and is almost completely erased by its parent's clip.
    auto panel_style = ButtonStyle(300.0f, 200.0f);
    panel_style.width = atom::layout::Length::Points(220.0f);
    panel_style.height = atom::layout::Length::Points(160.0f);
    const auto panel = tree.CreatePanel(tree.Root(), panel_style);
    const auto nested = tree.CreateButton(panel, "nested", ButtonStyle(12.0f, 18.0f));
    ATOM_CHECK(tree.Layout(800.0f, 600.0f));
    const auto nested_bounds = tree.GetBounds(nested);
    ATOM_CHECK(nested_bounds.has_value());
    ATOM_CHECK(nested_bounds->x == 312.0f);
    ATOM_CHECK(nested_bounds->y == 218.0f);

    // Later siblings render on top and must also win hit testing.
    ATOM_CHECK(tree.HandleEvent(MouseEvent(atom::window::EventType::MouseButtonPressed, 30.0f, 30.0f)));
    ATOM_CHECK(tree.GetFocusedNode() == front);
    ATOM_CHECK(atom::input::InputSystem::GetInstance().IsMouseCapturedByUi());
    ATOM_CHECK(tree.HandleEvent(MouseEvent(atom::window::EventType::MouseButtonReleased, 30.0f, 30.0f)));
    ATOM_CHECK(back_clicks == 0);
    ATOM_CHECK(front_clicks == 1);
    tree.Update(); // Runtime calls page Update after fixed simulation.
    ATOM_CHECK(!atom::input::InputSystem::GetInstance().IsMouseCapturedByUi());

    // Focused buttons accept keyboard activation and claim that key frame, so
    // a Space click cannot also trigger an underlying game action.
    atom::window::IEvent key{};
    key.type = atom::window::EventType::KeyPressed;
    key.data = atom::window::KeyEvent{atom::input::Key::Space, atom::input::KeyModifiers::None, false};
    ATOM_CHECK(tree.HandleEvent(key));
    ATOM_CHECK(front_clicks == 2);
    ATOM_CHECK(atom::input::InputSystem::GetInstance().IsKeyboardCapturedByUi());
    tree.Update();
    ATOM_CHECK(!atom::input::InputSystem::GetInstance().IsKeyboardCapturedByUi());

    ATOM_CHECK(tree.SetEnabled(front, false));
    // Disabled widgets do not intercept input; the visible button below is
    // now the hit target rather than a dead click-through blocker.
    ATOM_CHECK(tree.HandleEvent(MouseEvent(atom::window::EventType::MouseButtonPressed, 30.0f, 30.0f)));
    ATOM_CHECK(tree.GetFocusedNode() == back);
    ATOM_CHECK(tree.HandleEvent(MouseEvent(atom::window::EventType::MouseButtonReleased, 30.0f, 30.0f)));
    ATOM_CHECK(back_clicks == 1);
    ATOM_CHECK(front_clicks == 2);

    // Modal UI consumes an otherwise empty part of the canvas and claims the
    // raw device channel until Runtime has completed this frame's fixed step.
    tree.SetModal(true);
    ATOM_CHECK(tree.HandleEvent(MouseEvent(atom::window::EventType::MouseButtonPressed, 700.0f, 500.0f)));
    ATOM_CHECK(atom::input::InputSystem::GetInstance().IsMouseCapturedByUi());
    tree.Update();
    ATOM_CHECK(!atom::input::InputSystem::GetInstance().IsMouseCapturedByUi());

    return 0;
}
