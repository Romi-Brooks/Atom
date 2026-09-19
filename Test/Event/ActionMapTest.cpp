#include <Event/ActionMap.hpp>
#include <Test/Support/TestHelpers.hpp>

namespace {

enum class Action { Jump, Pause };

} // namespace

auto main() -> int {
    using namespace atom::event;

    ActionMap<Action> map{};
    ATOM_CHECK(map.Bind({Key::Space, KeyModifiers::None}, Action::Jump));
    ATOM_CHECK(map.Bind({Key::Escape, KeyModifiers::None}, Action::Pause));
    ATOM_CHECK(!map.Bind({Key::Unknown, KeyModifiers::None}, Action::Jump));
    ATOM_CHECK(map.GetBindings().size() == 2);

    const KeyEvent space{Key::Space, KeyModifiers::None, false};
    const KeyEvent ctrl_s{Key::S, KeyModifiers::Control, false};
    const auto jump = map.FindAction(space);
    ATOM_CHECK(jump.has_value() && *jump == Action::Jump);
    ATOM_CHECK(!map.FindAction(ctrl_s).has_value());

    ATOM_CHECK(map.Bind({Key::Space, KeyModifiers::None}, Action::Pause));
    ATOM_CHECK(map.GetBindings().size() == 2);
    const auto rebound = map.FindAction(space);
    ATOM_CHECK(rebound.has_value() && *rebound == Action::Pause);

    ATOM_CHECK(map.Bind({Key::W, KeyModifiers::None}, Action::Jump));
    ATOM_CHECK(map.Bind({Key::Up, KeyModifiers::None}, Action::Jump));
    ATOM_CHECK(map.GetBindings().size() == 4);
    const auto w = map.FindAction(KeyEvent{Key::W, KeyModifiers::None, false});
    const auto up = map.FindAction(KeyEvent{Key::Up, KeyModifiers::None, false});
    ATOM_CHECK(w.has_value() && *w == Action::Jump);
    ATOM_CHECK(up.has_value() && *up == Action::Jump);

    map.Unbind({Key::W, KeyModifiers::None});
    ATOM_CHECK(!map.FindAction(KeyEvent{Key::W, KeyModifiers::None, false}).has_value());
    map.Clear();
    ATOM_CHECK(map.GetBindings().empty());

    ATOM_CHECK(map.Bind({Key::A, KeyModifiers::Control}, Action::Pause));
    ATOM_CHECK(map.FindAction(KeyEvent{Key::A, KeyModifiers::Control, false}).has_value());
    ATOM_CHECK(!map.FindAction(KeyEvent{Key::A, KeyModifiers::None, false}).has_value());
    ATOM_CHECK(HasModifier(KeyModifiers::Control | KeyModifiers::Shift, KeyModifiers::Control));
    return 0;
}
