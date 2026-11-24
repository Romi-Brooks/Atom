# UI (P0)

`Atom_UI` is Atom's retained-mode game UI. It uses `LayoutTree` for Flexbox
geometry, `InputSystem` for aggregate UI capture, and `Renderer2D` only for
screen-space drawing. It does not depend on Runtime, Window implementations,
ECS, or ImGui.

The owning game page creates a `UiTree`, rebuilds its layout whenever output
size changes, sends it normalized events, calls `Update()` from `Screen::Update`,
and draws it with `UiCanvas::RenderFrame()`.

P0 widgets are `Panel`, `Label`, and `Button`. Buttons support hover, pressed,
disabled state, pointer capture, click callbacks, and Enter/Space activation
when focused. `TextField`, IME composition, scrolling, image/nine-slice, and
gamepad navigation are intentionally later milestones.
