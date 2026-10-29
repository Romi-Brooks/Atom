/**
 * @file SliceSelfTest.hpp
 * @brief Headless verification of the vertical slice, run by `Example_2D_Smoke --selftest`.
 *
 * It is a plain function rather than a test target because the thing under test is the *example*
 * assembled exactly as the game assembles it - scene document, physics bridge, systems, input.
 * Rebuilding that inside a unit test would test a copy, and the copy is what stays green while
 * the game breaks.
 */

#ifndef ATOM_EXAMPLE_GAME_SLICESELFTEST_HPP
#define ATOM_EXAMPLE_GAME_SLICESELFTEST_HPP

namespace atom::game {

// Returns 0 when every slice assertion holds, 1 otherwise (with the reason logged).
[[nodiscard]] auto RunSliceSelfTest() -> int;

} // namespace atom::game

#endif // ATOM_EXAMPLE_GAME_SLICESELFTEST_HPP
