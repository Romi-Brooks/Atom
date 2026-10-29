/**
 * @file TestGameMain.cpp
 * @brief T2.2 entry point: a window, the repository's Assets/ mounted as res://, and
 *        one GameScreen whose world is loaded from a scene document.
 *
 * The content-root convention (G0.5) is stated here rather than hidden in a helper:
 * development runs mount ATOM_SOURCE_ROOT/Example/TestGame/Assets at priority 200, so
 * `res://scenes/test_room.scene.json` and the sprite URIs inside it resolve to real
 * files in the checkout. A packaged build would mount the same URIs from game.apkg
 * and nothing above this point would change.
 *
 * Order matters: the scene is loaded *before* the screen is registered, so a broken
 * scene fails at boot with a diagnostic instead of showing an empty world and letting
 * the author wonder why nothing is drawn.
 *
 * Controls: WASD/arrows move, Space pauses the world (the scene keeps rendering),
 * window close quits.
 */

#include <memory>
#include <string>

#include <Filesystem/FileSystem.hpp>
#include <Filesystem/Vfs.hpp>
#include <Log/LogSystem.hpp>
#include <Runtime/Application.hpp>
#include <Runtime/ScreenManager.hpp>
#include <Window/RenderWindow.hpp>

#include "TestGameScreen.hpp"

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

// Mounts the development content root. Returns false when the Assets directory is
// missing, which is a boot failure worth reporting rather than a silent "no
// sprites" screen.
auto MountDevelopmentAssets() -> bool {
    auto& vfs = atom::fs::Vfs::GetInstance();
    std::unique_ptr<atom::fs::NativeFileSystem> backend{};
    const std::string root = std::string{ATOM_SOURCE_ROOT} + "/Example/TestGame/Assets";
    if (atom::fs::NativeFileSystem::Create("res", root, backend) != atom::fs::Result::Success || !backend) {
        LOG_ERROR(atom::log::core::Main, "TestGame: content root unavailable: " + root);
        return false;
    }
    // The VFS takes shared ownership; the unique_ptr has to be released into it.
    std::shared_ptr<atom::fs::IFileSystem> shared{std::move(backend)};
    if (vfs.Mount("res", 200, shared) != atom::fs::Result::Success) {
        LOG_ERROR(atom::log::core::Main, "TestGame: mounting res:// at " + root + " failed");
        return false;
    }
    LOG_INFO(atom::log::core::Main, "TestGame: mounted res:// -> " + root);
    return true;
}

} // namespace

auto main() -> int {
    atom::Log::SetConsoleOutputUtf8();
    atom::Log::SetViewLogLevel(atom::LogLevel::ATOM_INFO);

    if (!MountDevelopmentAssets())
        return 1;


    auto& screens = atom::ScreenManager::GetInstance();

    auto screen = std::make_unique<atom::game::TestGameScreen>();
    if (auto room = screen->LoadRoom(); !room.has_value()) {
        LOG_ERROR(atom::log::core::Main, "The scene failed to load: " + room.error().Message());
        return 1;
    }

    const auto loaded = screens.LoadScreen("TestGame", std::move(screen));
    if (loaded != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager,
                  "Failed to register the TestGame: " + std::string{atom::ToString(loaded)});
        return 1;
    }
    if (screens.SwitchScreen("TestGame") != atom::ScreenOpResult::Success) {
        LOG_ERROR(atom::log::core::ScreenManager, "Failed to switch to the TestGame");
        return 1;
    }

    auto& window = atom::RenderWindow::GetInstance();
    auto& application = atom::runtime::Application::GetInstance();
    const auto initialized = application.Initialize("Atom Engine - TestGame",
                                                    atom::algo::Vec2{static_cast<float>(kWindowWidth),
                                                                     static_cast<float>(kWindowHeight)},
                                                    atom::backend::RenderBackendId::SdlGpu);
    if (!initialized) {
        LOG_ERROR(atom::log::core::Main, "Window initialization failed: " + initialized.error().Message());
        return 1;
    }
    window.SetFPS(165);

    LOG_INFO(atom::log::core::Screen,
             "TestGame running: WASD/arrows move the player, Space pauses the world, close the window to quit.");

    application.Run();
    return 0;
}
