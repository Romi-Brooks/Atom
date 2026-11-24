/**
 * @file           : DisplayConfigTest.cpp
 * @brief          : The display/presentation selection contract
 *                   (WindowMode, PresentMode, RenderBackendConfig).
 *
 * Pure header coverage: no window, no GPU, no display. What is worth pinning
 * down here is the part that settings UI and game code branch on — the
 * refresh-paced predicate and the capability bitmask — because getting those
 * wrong is how "vsync off" silently turns into "still capped at 144".
 */

#include <Backend/Contracts/Render/PresentMode.hpp>
#include <Backend/Contracts/Render/RenderBackendConfig.hpp>
#include <Backend/Contracts/Window/WindowMode.hpp>
#include <Test/Support/TestHelpers.hpp>

auto main() -> int {
    // --- WindowMode ---------------------------------------------------------
    ATOM_CHECK(atom::window::ToString(atom::window::WindowMode::Windowed) == "windowed");
    ATOM_CHECK(atom::window::ToString(atom::window::WindowMode::Borderless) == "borderless");
    ATOM_CHECK(atom::window::ToString(atom::window::WindowMode::ExclusiveFullscreen) == "exclusive");

    // Only the exclusive placement is a real fullscreen state. Borderless is an
    // undecorated ordinary window that merely covers the display when sized to
    // it, and callers must not treat it as a display-mode change.
    ATOM_CHECK(!atom::window::IsFullscreen(atom::window::WindowMode::Windowed));
    ATOM_CHECK(!atom::window::IsFullscreen(atom::window::WindowMode::Borderless));
    ATOM_CHECK(atom::window::IsFullscreen(atom::window::WindowMode::ExclusiveFullscreen));

    ATOM_CHECK(!atom::window::IsUndecorated(atom::window::WindowMode::Windowed));
    ATOM_CHECK(atom::window::IsUndecorated(atom::window::WindowMode::Borderless));
    ATOM_CHECK(atom::window::IsUndecorated(atom::window::WindowMode::ExclusiveFullscreen));

    // --- PresentMode --------------------------------------------------------
    ATOM_CHECK(atom::render::ToString(atom::render::PresentMode::VSync) == "vsync");
    ATOM_CHECK(atom::render::ToString(atom::render::PresentMode::Mailbox) == "mailbox");
    ATOM_CHECK(atom::render::ToString(atom::render::PresentMode::Immediate) == "immediate");

    // The predicate that matters: Mailbox does NOT stall a submit, but it is
    // still paced to the display refresh, so it must never be reported as
    // "uncapped". Treating "!= VSync" as "not paced" is the classic bug this
    // asserts against.
    ATOM_CHECK(atom::render::IsRefreshPaced(atom::render::PresentMode::VSync));
    ATOM_CHECK(atom::render::IsRefreshPaced(atom::render::PresentMode::Mailbox));
    ATOM_CHECK(!atom::render::IsRefreshPaced(atom::render::PresentMode::Immediate));

    // --- capability bitmask -------------------------------------------------
    const auto vsync_bit = atom::render::ToMask(atom::render::PresentMode::VSync);
    const auto mailbox_bit = atom::render::ToMask(atom::render::PresentMode::Mailbox);
    const auto immediate_bit = atom::render::ToMask(atom::render::PresentMode::Immediate);
    ATOM_CHECK(vsync_bit != mailbox_bit);
    ATOM_CHECK(mailbox_bit != immediate_bit);
    ATOM_CHECK(vsync_bit != immediate_bit);
    ATOM_CHECK(atom::render::all_present_modes == static_cast<std::uint8_t>(vsync_bit | mailbox_bit | immediate_bit));

    ATOM_CHECK(atom::render::SupportsPresentMode(atom::render::all_present_modes, atom::render::PresentMode::VSync));
    ATOM_CHECK(atom::render::SupportsPresentMode(atom::render::all_present_modes, atom::render::PresentMode::Mailbox));
    ATOM_CHECK(
        atom::render::SupportsPresentMode(atom::render::all_present_modes, atom::render::PresentMode::Immediate));

    // VSync is the only mode every backend is required to support.
    const std::uint8_t vsync_only = vsync_bit;
    ATOM_CHECK(atom::render::SupportsPresentMode(vsync_only, atom::render::PresentMode::VSync));
    ATOM_CHECK(!atom::render::SupportsPresentMode(vsync_only, atom::render::PresentMode::Mailbox));
    ATOM_CHECK(!atom::render::SupportsPresentMode(vsync_only, atom::render::PresentMode::Immediate));
    ATOM_CHECK(!atom::render::SupportsPresentMode(0, atom::render::PresentMode::VSync));

    // Every mode has to be representable in the mask.
    for (std::size_t index = 0; index < atom::render::present_mode_count; ++index) {
        const auto mode = static_cast<atom::render::PresentMode>(index);
        ATOM_CHECK(atom::render::SupportsPresentMode(atom::render::all_present_modes, mode));
        ATOM_CHECK(atom::render::ToMask(mode) != 0);
    }

    // --- GpuDriver ----------------------------------------------------------
    ATOM_CHECK(atom::backend::ToString(atom::backend::GpuDriver::Auto) == "auto");
    ATOM_CHECK(atom::backend::ToString(atom::backend::GpuDriver::Direct3D12) == "direct3d12");
    ATOM_CHECK(atom::backend::ToString(atom::backend::GpuDriver::Vulkan) == "vulkan");
    ATOM_CHECK(atom::backend::ToString(atom::backend::GpuDriver::Metal) == "metal");

    // --- ValidationMode -----------------------------------------------------
    // Auto must stay the default: it is the only value that lets a Debug build
    // validate while a shipped build does not. Forcing either extreme in the
    // default would either ship a slow engine or silently drop validation.
    ATOM_CHECK(atom::backend::ToString(atom::backend::ValidationMode::Auto) == "auto");
    ATOM_CHECK(atom::backend::ToString(atom::backend::ValidationMode::Off) == "off");
    ATOM_CHECK(atom::backend::ToString(atom::backend::ValidationMode::On) == "on");
    ATOM_CHECK(atom::backend::RenderBackendConfig{}.validation == atom::backend::ValidationMode::Auto);

    // --- RenderBackendConfig ------------------------------------------------
    // Defaults are the conservative desktop case: a game that says nothing gets
    // a windowed, vsync-paced window on the backend's preferred driver. Change
    // one of these and every existing title silently changes behaviour.
    const atom::backend::RenderBackendConfig defaults{};
    ATOM_CHECK(defaults.backend == atom::backend::RenderBackendId::SdlGpu);
    ATOM_CHECK(defaults.driver == atom::backend::GpuDriver::Auto);
    ATOM_CHECK(defaults.window_mode == atom::window::WindowMode::Windowed);
    ATOM_CHECK(defaults.present_mode == atom::render::PresentMode::VSync);
    ATOM_CHECK(defaults.resolution.GetX() > 0.0f);
    ATOM_CHECK(defaults.resolution.GetY() > 0.0f);

    // The axes are independent: asking for Vulkan is not a backend change, and
    // asking for another backend is not a driver change.
    auto vulkan_request = atom::backend::RenderBackendConfig{};
    vulkan_request.driver = atom::backend::GpuDriver::Vulkan;
    ATOM_CHECK(vulkan_request.backend == atom::backend::RenderBackendId::SdlGpu);
    ATOM_CHECK(atom::backend::ToString(vulkan_request.backend) == "sdl_gpu");

    return 0;
}
