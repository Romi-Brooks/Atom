#include <Layout/LayoutTree.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::layout;

    LayoutTree tree{};
    ATOM_CHECK(tree.Root() != LayoutTree::kInvalidNode);
    ATOM_CHECK(tree.GetNode(tree.Root()) != nullptr);
    ATOM_CHECK(tree.GetNode(LayoutTree::kInvalidNode) == nullptr);

    const auto child = tree.CreateNode();
    ATOM_CHECK(child != LayoutTree::kInvalidNode);
    ATOM_CHECK(child != tree.Root());
    ATOM_CHECK(tree.GetNode(child) != nullptr);

    LayoutStyle child_style{};
    child_style.width = Length::Points(100.0f);
    child_style.height = Length::Points(40.0f);
    ATOM_CHECK(tree.SetStyle(child, child_style));
    ATOM_CHECK(tree.Append(tree.Root(), child));

    LayoutStyle root_style{};
    root_style.width = Length::Points(200.0f);
    root_style.height = Length::Points(100.0f);
    root_style.flex_direction = FlexDirection::Column;
    ATOM_CHECK(tree.SetStyle(tree.Root(), root_style));

    ATOM_CHECK(tree.Calculate(200.0f, 100.0f, Direction::LeftToRight));
    const auto root_layout = tree.GetLayout(tree.Root());
    const auto child_layout = tree.GetLayout(child);
    ATOM_CHECK(root_layout.has_value());
    ATOM_CHECK(child_layout.has_value());
    ATOM_CHECK(atom::test::NearlyEqual(root_layout->width, 200.0f, 0.5f));
    ATOM_CHECK(atom::test::NearlyEqual(root_layout->height, 100.0f, 0.5f));
    ATOM_CHECK(atom::test::NearlyEqual(child_layout->width, 100.0f, 0.5f));
    ATOM_CHECK(atom::test::NearlyEqual(child_layout->height, 40.0f, 0.5f));

    const auto sibling = tree.CreateNode();
    LayoutStyle sibling_style{};
    sibling_style.width = Length::Points(50.0f);
    sibling_style.height = Length::Points(20.0f);
    ATOM_CHECK(tree.SetStyle(sibling, sibling_style));
    ATOM_CHECK(tree.Insert(tree.Root(), sibling, 0));
    ATOM_CHECK(tree.Calculate(200.0f, 100.0f));
    const auto sibling_layout = tree.GetLayout(sibling);
    ATOM_CHECK(sibling_layout.has_value());
    ATOM_CHECK(sibling_layout->top <= child_layout->top + 0.5f);

    ATOM_CHECK(tree.Remove(tree.Root(), sibling));
    ATOM_CHECK(!tree.Append(child, tree.Root())); // root is an ancestor of child
    ATOM_CHECK(!tree.Append(tree.Root(), tree.Root()));
    ATOM_CHECK(tree.DestroyNode(sibling));
    ATOM_CHECK(tree.GetNode(sibling) == nullptr);
    ATOM_CHECK(!tree.DestroyNode(sibling));
    ATOM_CHECK(!tree.SetStyle(sibling, sibling_style));
    ATOM_CHECK(!tree.Append(tree.Root(), sibling));
    return 0;
}
