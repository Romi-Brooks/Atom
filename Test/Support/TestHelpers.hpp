/**
 * @file           : TestHelpers.hpp
 * @brief          : Shared fail/assert helpers for Atom-owned CTest targets.
 */

#ifndef ATOM_TEST_SUPPORT_TEST_HELPERS_HPP
#define ATOM_TEST_SUPPORT_TEST_HELPERS_HPP

#include <cmath>
#include <iostream>
#include <string_view>

namespace atom::test {

inline auto Fail(const std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

[[nodiscard]] inline auto NearlyEqual(const float left, const float right, const float epsilon = 1.0e-5f) -> bool {
    return std::abs(left - right) <= epsilon * std::max({1.0f, std::abs(left), std::abs(right)});
}

#define ATOM_CHECK(...)                                                                                                \
    do {                                                                                                               \
        if (!(__VA_ARGS__))                                                                                            \
            return atom::test::Fail("Check failed: " #__VA_ARGS__);                                                    \
    } while (false)

#define ATOM_CHECK_MSG(cond, message)                                                                                  \
    do {                                                                                                               \
        if (!(cond))                                                                                                   \
            return atom::test::Fail(message);                                                                          \
    } while (false)

#define ATOM_CHECK_NEAR(...)                                                                                           \
    do {                                                                                                               \
        if (!atom::test::NearlyEqual(__VA_ARGS__))                                                                     \
            return atom::test::Fail("Near check failed: " #__VA_ARGS__);                                               \
    } while (false)

} // namespace atom::test

#endif // ATOM_TEST_SUPPORT_TEST_HELPERS_HPP
