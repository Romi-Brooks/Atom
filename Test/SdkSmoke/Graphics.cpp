#include <Atom/Presentation/Layout/LayoutTree.hpp>

auto main() -> int {
    atom::layout::LayoutTree tree;
    const auto child = tree.CreateNode();
    atom::layout::LayoutStyle style;
    style.width = atom::layout::Length::Points(100.0F);
    style.height = atom::layout::Length::Points(40.0F);
    if (!tree.SetStyle(child, style) || !tree.Append(tree.Root(), child) || !tree.Calculate(200.0F, 100.0F)) {
        return 1;
    }

    const auto layout = tree.GetLayout(child);
    return layout && layout->width == 100.0F && layout->height == 40.0F ? 0 : 2;
}
