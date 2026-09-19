#include <Algorithm/Color/Color.hpp>
#include <Algorithm/Color/ColorMath.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using atom::color::ApplyOpacity;
    using atom::color::Blend;
    using atom::color::Color;

    ATOM_CHECK(Color::Black().r == 0 && Color::Black().g == 0 && Color::Black().b == 0 && Color::Black().a == 255);
    ATOM_CHECK(Color::White().r == 255 && Color::White().a == 255);
    ATOM_CHECK(Color::Red().r == 255 && Color::Red().g == 0 && Color::Red().b == 0);
    ATOM_CHECK(Color::Green().g == 255);
    ATOM_CHECK(Color::Blue().b == 255);

    const auto half = ApplyOpacity(Color::White(), 0.5f);
    ATOM_CHECK(half.a == 127);
    ATOM_CHECK(ApplyOpacity(Color::White(), 0.0f).a == 0);
    ATOM_CHECK(ApplyOpacity(Color::White(), 2.0f).a == 255);
    ATOM_CHECK(ApplyOpacity(Color::White(), -1.0f).a == 0);

    const auto mid = Blend(Color::Red(), Color::Blue(), 0.0f);
    ATOM_CHECK(mid.r == 255 && mid.b == 0);
    const auto end = Blend(Color::Red(), Color::Blue(), 1.0f);
    ATOM_CHECK(end.r == 0 && end.b == 255);
    const auto center = Blend(Color::Black(), Color::White(), 0.5f);
    ATOM_CHECK(center.r >= 127 && center.r <= 128);
    const auto over = Blend(Color::Black(), Color::White(), 2.0f);
    ATOM_CHECK(over.r == 255 && over.g == 255 && over.b == 255);
    return 0;
}
