# Layout

Atom's layout module exposes a renderer-independent Flexbox API backed by
[Yoga](https://www.yogalayout.dev/). Engine and game code depend on
`atom::layout` types rather than Yoga headers, so the implementation can be
replaced without changing render backends or UI components.

## Usage

```cpp
atom::layout::LayoutConfig config;
atom::layout::LayoutNode root{config};
atom::layout::LayoutNode content{config};

auto root_style = atom::layout::LayoutStyle{};
root_style.width = atom::layout::Length::Points(1280.0f);
root_style.height = atom::layout::Length::Points(720.0f);
root.SetStyle(root_style);

auto content_style = atom::layout::LayoutStyle{};
content_style.flex_grow = 1.0f;
content.SetStyle(content_style);

root.AppendChild(content);
root.CalculateLayout();
const auto content_bounds = content.GetLayout();
```

Use one shared `LayoutConfig` for nodes in the same UI tree. It defaults to web
Flexbox behavior. Set its point scale factor to the output scale when layout
results should be rounded to physical pixels, or to `0` to disable rounding.

Yoga calculates boxes only. Rendering, hit testing, clipping, focus, scrolling,
and animation remain responsibilities of Atom's future UI layer. Text and other
intrinsic-size leaves provide a `LayoutNode::MeasureFunction`; call `MarkDirty()`
after their measured content changes.

`LayoutNode` does not own its children. Keep every child alive while it is
attached to a parent, or remove it before its lifetime ends. Destruction safely
disconnects a node from its Yoga tree.
