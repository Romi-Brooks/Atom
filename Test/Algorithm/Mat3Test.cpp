#include <Algorithm/Math/Scalar.hpp>
#include <Algorithm/Matrix/Mat3.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    using namespace atom::algo;

    const auto identity = Mat3::Identity();
    const Vec2 point{2.0f, 3.0f};
    const auto same = identity.TransformPoint(point);
    ATOM_CHECK_NEAR(same.GetX(), 2.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(same.GetY(), 3.0f, 1.0e-5f);

    const auto translated = Mat3::Translation({10.0f, -4.0f}).TransformPoint(point);
    ATOM_CHECK_NEAR(translated.GetX(), 12.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(translated.GetY(), -1.0f, 1.0e-5f);

    const auto scaled = Mat3::Scale({2.0f, 3.0f}).TransformPoint(point);
    ATOM_CHECK_NEAR(scaled.GetX(), 4.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(scaled.GetY(), 9.0f, 1.0e-5f);

    const auto rotated = Mat3::Rotation(Pi * 0.5f).TransformPoint(Vec2{1.0f, 0.0f});
    ATOM_CHECK_NEAR(rotated.GetX(), 0.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(rotated.GetY(), 1.0f, 1.0e-5f);

    const auto composed = Mat3::Translation({1.0f, 1.0f}) * Mat3::Scale({2.0f, 2.0f});
    const auto composed_point = composed.TransformPoint(Vec2{1.0f, 1.0f});
    ATOM_CHECK_NEAR(composed_point.GetX(), 3.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(composed_point.GetY(), 3.0f, 1.0e-5f);

    ATOM_CHECK_NEAR(Mat3::Scale({2.0f, 4.0f}).Determinant(), 8.0f, 1.0e-5f);
    ATOM_CHECK(!Mat3::Scale({0.0f, 1.0f}).Inverse().has_value());

    const auto inverse = Mat3::Translation({5.0f, -2.0f}).Inverse();
    ATOM_CHECK(inverse.has_value());
    const auto round_trip = (*inverse * Mat3::Translation({5.0f, -2.0f})).TransformPoint(point);
    ATOM_CHECK_NEAR(round_trip.GetX(), point.GetX(), 1.0e-4f);
    ATOM_CHECK_NEAR(round_trip.GetY(), point.GetY(), 1.0e-4f);

    const Vec2 vector{1.0f, 0.0f};
    const auto rotated_vector = Mat3::Translation({100.0f, 100.0f}).TransformVector(vector);
    ATOM_CHECK_NEAR(rotated_vector.GetX(), 1.0f, 1.0e-5f);
    ATOM_CHECK_NEAR(rotated_vector.GetY(), 0.0f, 1.0e-5f);
    return 0;
}
