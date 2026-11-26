#include "GameOverlay.hpp"

#include <algorithm>
#include <array>

#include <Atom/Platform/Display/DisplaySystem.hpp>
#include <Atom/Runtime/Application.hpp>
#include <Atom/Platform/Window/RenderWindow.hpp>

namespace homeless {

namespace {

constexpr auto ink = atom::color::Color{14, 27, 33, 255};
constexpr auto highlight = atom::color::Color{54, 82, 91, 255};
constexpr auto line = atom::color::Color{106, 145, 151, 255};
constexpr auto lcd = atom::color::Color{151, 185, 161, 255};

constexpr std::array modes{atom::window::WindowMode::ExclusiveFullscreen, atom::window::WindowMode::Borderless,
                           atom::window::WindowMode::Windowed};
constexpr std::array mode_names{"Fullscreen", "Borderless", "Windowed"};
constexpr std::array present_modes{atom::render::PresentMode::Immediate, atom::render::PresentMode::VSync,
                                   atom::render::PresentMode::Mailbox};
constexpr std::array present_names{"Immediate", "VSync", "Mailbox"};
constexpr std::array<unsigned, 6> frame_rates{0, 30, 60, 120, 144, 240};

template <typename Values, typename Value> auto IndexOf(const Values& values, const Value& value) -> int {
    return static_cast<int>(std::ranges::find(values, value) - values.begin());
}

auto Wrap(int index, int count) -> int {
    return (index % count + count) % count;
}

auto SizeLabel(atom::algo::Vec2 size) -> std::string {
    return std::to_string(static_cast<unsigned>(size.GetX())) + " x " +
           std::to_string(static_cast<unsigned>(size.GetY()));
}

} // namespace

auto GameOverlay::MenuRect() const -> atom::algo::Rect {
    const float width = page_ == Page::Settings ? 268.0f : 180.0f;
    const float height = page_ == Page::Settings ? 239.0f : 160.0f;
    return {(ui_size_.GetX() - width) * 0.5f, (ui_size_.GetY() - height) * 0.5f, width, height};
}

auto GameOverlay::MenuRow(int index) const -> atom::algo::Rect {
    const auto panel = MenuRect();
    const float row_height = page_ == Page::Settings ? 25.0f : 30.0f;
    return {panel.x + 12, panel.y + 40 + index * row_height, panel.width - 24, row_height - 4};
}

auto GameOverlay::HandleEvent(const atom::window::IEvent& event) -> bool {
    const auto* key = std::get_if<atom::window::KeyEvent>(&event.data);
    if (event.type == atom::window::EventType::KeyPressed && key && !key->is_repeat) {
        if (key->key == atom::input::Key::F11) {
            auto& display = atom::display::DisplaySystem::GetInstance();
            using atom::window::WindowMode;
            const auto mode = display.Requested().window_mode;
            display.RequestWindowMode(mode == WindowMode::ExclusiveFullscreen ? WindowMode::Windowed
                                                                              : WindowMode::ExclusiveFullscreen);
            return true;
        }
        if (key->key == atom::input::Key::Escape) {
            page_ = page_ == Page::Settings ? Page::Pause : page_ == Page::Pause ? Page::Game : Page::Pause;
            return true;
        }
        if (key->key == atom::input::Key::Space && page_ != Page::Settings) {
            page_ = page_ == Page::Game ? Page::Pause : Page::Game;
            return true;
        }
    }

    const auto* mouse = std::get_if<atom::window::MouseEvent>(&event.data);
    if (!mouse)
        return false;
    mouse_ = ToUi(mouse->x, mouse->y);
    if (event.type != atom::window::EventType::MouseButtonPressed || mouse->button != atom::input::MouseButton::Left)
        return IsMenuOpen();

    if (page_ == Page::Game) {
        return HandlePlayerClick();
    }

    if (page_ == Page::Pause) {
        constexpr std::array actions{Action::Continue, Action::Settings, Action::Quit};
        for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
            if (MenuRow(i).Contains(mouse_)) {
                Activate(actions[i], 0);
                return true;
            }
        }
    } else {
        constexpr std::array actions{Action::WindowMode, Action::Resolution, Action::FrameRate, Action::PresentMode,
                                     Action::Sfx,        Action::Music,      Action::Back};
        for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
            const auto row = MenuRow(i);
            if (row.Contains(mouse_) && (actions[i] == Action::Back || mouse_.GetX() >= row.x + row.width * 0.5f)) {
                Activate(actions[i], mouse_.GetX() < row.x + row.width * 0.75f ? -1 : 1);
                return true;
            }
        }
    }
    return true;
}

auto GameOverlay::Activate(Action action, int direction) -> void {
    auto& display = atom::display::DisplaySystem::GetInstance();
    auto& window = atom::RenderWindow::GetInstance();
    const auto applied = display.Applied();
    switch (action) {
    case Action::Continue:
        page_ = Page::Game;
        break;
    case Action::Settings:
        page_ = Page::Settings;
        break;
    case Action::Quit:
        atom::runtime::Application::GetInstance().RequestStop();
        break;
    case Action::Back:
        page_ = Page::Pause;
        break;
    case Action::WindowMode:
        display.RequestWindowMode(modes[Wrap(IndexOf(modes, applied.window_mode) + direction, modes.size())]);
        break;
    case Action::Resolution: {
        auto& platform_window = *window.GetIWindow();
        const bool fullscreen = applied.window_mode == atom::window::WindowMode::ExclusiveFullscreen;
        auto choices = platform_window.GetFullscreenResolutions();
        if (!fullscreen) {
            for (const auto size : {atom::algo::Vec2{960, 540}, atom::algo::Vec2{1120, 630},
                                    atom::algo::Vec2{1280, 720}, atom::algo::Vec2{1600, 900}}) {
                if (std::ranges::find(choices, size) == choices.end())
                    choices.push_back(size);
            }
        }
        if (choices.empty())
            break;
        std::ranges::sort(choices, {}, [](const auto& size) { return size.LengthSquared(); });
        const auto current = fullscreen ? platform_window.GetSize() : platform_window.GetCoordinateSize();
        const auto position = IndexOf(choices, current);
        const auto target = choices[Wrap(position + direction, static_cast<int>(choices.size()))];
        display.RequestResolution(static_cast<unsigned>(target.GetX()), static_cast<unsigned>(target.GetY()));
        break;
    }
    case Action::FrameRate:
        window.SetFPS(frame_rates[Wrap(IndexOf(frame_rates, window.GetFPS()) + direction, frame_rates.size())]);
        break;
    case Action::PresentMode: {
        const auto supported = window.GetRenderDevice()->SupportedPresentModes();
        int index = IndexOf(present_modes, applied.present_mode);
        for (std::size_t i = 0; i < present_modes.size(); ++i) {
            index = Wrap(index + direction, present_modes.size());
            if (atom::render::SupportsPresentMode(supported, present_modes[index])) {
                display.RequestPresentMode(present_modes[index]);
                break;
            }
        }
        break;
    }
    case Action::Music:
        mixer_.SetMusicVolume(std::clamp(mixer_.GetMusicVolume() + direction * 10.0f, 0.0f, 100.0f));
        break;
    case Action::Sfx:
        mixer_.SetSFXVolume(std::clamp(mixer_.GetSFXVolume() + direction * 10.0f, 0.0f, 100.0f));
        break;
    }
}

auto GameOverlay::Button(const atom::algo::Rect& bounds, std::string_view label) -> void {
    Fill(bounds, bounds.Contains(mouse_) ? highlight : atom::color::Color{34, 54, 62, 255});
    Outline(bounds, line);
    Text(label, bounds.x + 8, bounds.y + (bounds.height - 12.0f) * 0.5f, 11);
}

auto GameOverlay::DrawMenu() -> void {
    Fill({0, 0, ui_size_.GetX(), ui_size_.GetY()}, {7, 12, 25, 185});
    const auto panel = MenuRect();
    Fill(panel, ink);
    Outline(panel, line);
    Fill({panel.x, panel.y, panel.width, 2}, lcd);
    Text(page_ == Page::Pause ? "Paused" : "Settings", panel.x + 12, panel.y + 12, 17);
    if (page_ == Page::Pause) {
        Button(MenuRow(0), "Continue");
        Button(MenuRow(1), "Settings");
        Button(MenuRow(2), "Quit");
        return;
    }

    auto& window = atom::RenderWindow::GetInstance();
    const auto applied = atom::display::DisplaySystem::GetInstance().Applied();
    const auto fps = window.GetFPS();
    const std::array<std::string, 6> values{
        mode_names[IndexOf(modes, applied.window_mode)],
        SizeLabel(window.GetRenderDevice()->GetOutputSize()),
        fps == 0 ? "Unlimited" : std::to_string(fps),
        present_names[IndexOf(present_modes, applied.present_mode)],
        std::to_string(static_cast<int>(mixer_.GetSFXVolume())) + "%",
        std::to_string(static_cast<int>(mixer_.GetMusicVolume())) + "%",
    };
    constexpr std::array labels{"Window Mode",  "Resolution", "Frame Rate",
                                "Present Mode", "SFX Volume", "Music Volume"};
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        const auto row = MenuRow(i);
        Button(row, labels[i]);
        const float value_left = row.x + row.width * 0.54f;
        Text("<", value_left, row.y + 4, 10);
        Text(values[i], value_left + 13, row.y + 4, 10);
        Text(">", row.x + row.width - 12, row.y + 4, 10);
    }
    Button(MenuRow(6), "Back");
}

} // namespace homeless
