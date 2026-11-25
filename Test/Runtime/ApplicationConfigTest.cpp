/**
 * @file ApplicationConfigTest.cpp
 * @brief Startup-default contract for ApplicationConfig.
 *
 * Pure header coverage: no window, no GPU. What is worth pinning down is the
 * one engine-wide frame-cap default. If it silently becomes 0, every existing
 * title turns into an uncapped loop, and that is a behaviour change a default
 * must not make.
 */

#include <Atom/Runtime/Application.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    // The software frame cap starts from the window contract's constant, so the
    // facade (RenderWindow::fps_) and the startup config cannot drift apart.
    ATOM_CHECK(atom::runtime::ApplicationConfig{}.fps == atom::window::kDefaultFpsLimit);

    // 0 is a meaningful value (uncapped), not "unset": it stays a plain field
    // rather than becoming an optional that no longer distinguishes the two.
    atom::runtime::ApplicationConfig uncapped{};
    uncapped.fps = 0;
    ATOM_CHECK(uncapped.fps == 0);

    // The rest of the startup defaults stay the conservative desktop case.
    const atom::runtime::ApplicationConfig defaults{};
    ATOM_CHECK(defaults.window_mode == atom::window::WindowMode::Windowed);
    ATOM_CHECK(defaults.present_mode == atom::render::PresentMode::VSync);
    ATOM_CHECK(defaults.render_backend == atom::backend::RenderBackendId::SdlGpu);
    ATOM_CHECK(defaults.frames_in_flight > 0);

    return 0;
}
