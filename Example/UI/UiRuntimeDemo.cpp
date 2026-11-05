/**
 * @file UiRuntimeDemo.cpp
 * @brief Game-UI P0 acceptance example and Runtime Screen flow reference.
 *
 * Flow: Main Menu --Switch--> Settings / Game --Push--> Pause --Pop--> Game.
 * The ScreenManager owns page navigation; each page owns only its local UiTree.
 */

#include <Runtime/Application.hpp>
#include <Runtime/Screen.hpp>
#include <Runtime/ScreenManager.hpp>

#include <Render/Renderer2D/Renderer2D.hpp>
#include <UI/UiCanvas.hpp>
#include <UI/UiTree.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace {

using atom::layout::LayoutStyle;
using atom::render::Color;

[[nodiscard]] auto Box(const float width, const float height) -> LayoutStyle {
    LayoutStyle style{};
    style.width = atom::layout::Length::Points(width);
    style.height = atom::layout::Length::Points(height);
    return style;
}

[[nodiscard]] auto CardStyle(const float width, const float height) -> LayoutStyle {
    auto style = Box(width, height);
    style.flex_direction = atom::layout::FlexDirection::Column;
    style.row_gap = 14.0f;
    style.padding = atom::layout::Edges::All(atom::layout::Length::Points(26.0f));
    return style;
}

[[nodiscard]] auto Fill(const Color color, const bool clip = false) -> atom::ui::VisualStyle {
    atom::ui::VisualStyle style{};
    style.background = color;
    style.draw_background = true;
    style.clip_children = clip;
    return style;
}

[[nodiscard]] auto ButtonVisual() -> atom::ui::VisualStyle {
    atom::ui::VisualStyle style{};
    style.background = Color{57, 85, 148, 255};
    style.hover_background = Color{76, 112, 186, 255};
    style.pressed_background = Color{40, 62, 112, 255};
    style.disabled_background = Color{70, 70, 78, 255};
    style.text = Color{245, 248, 255, 255};
    style.text_size = 21.0f;
    style.draw_background = true;
    return style;
}

// The renderer and its font must outlive individual Screens, but must still
// release GPU resources before Runtime tears the render device down. main()
// registers Shutdown() as a Runtime shutdown observer for exactly that order.
class UiSurface final {
    public:
        ~UiSurface() { Shutdown(); }

        auto Draw(atom::render::IRenderDevice& device, atom::ui::UiTree& tree) -> void {
            if (!EnsureInitialized(device))
                return;
            const auto output = device.GetOutputSize();
            if (!tree.Layout(output.GetX(), output.GetY()))
                return;
            static_cast<void>(atom::ui::UiCanvas::RenderFrame(renderer_, tree, font_));
        }

        auto Shutdown() -> void {
            if (renderer_.IsInitialized())
                renderer_.Shutdown();
            font_ = nullptr;
            font_bytes_.clear();
        }

    private:
        auto EnsureInitialized(atom::render::IRenderDevice& device) -> bool {
            if (renderer_.IsInitialized())
                return true;
            if (!renderer_.Initialize(device, ATOM_SHADER_OUTPUT_DIR))
                return false;
            LoadFont(); // UI stays functional if no system font is available.
            return true;
        }

        auto LoadFont() -> void {
#ifdef _WIN32
            constexpr std::array candidates{"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"};
#else
            constexpr std::array candidates{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"};
#endif
            for (const auto* path : candidates) {
                if (!std::filesystem::exists(path))
                    continue;
                std::ifstream file{path, std::ios::binary | std::ios::ate};
                if (!file)
                    continue;
                const auto size = file.tellg();
                if (size <= 0)
                    continue;
                font_bytes_.resize(static_cast<std::size_t>(size));
                file.seekg(0, std::ios::beg);
                if (file.read(reinterpret_cast<char*>(font_bytes_.data()), size) &&
                    (font_ = renderer_.LoadFontFromMemory(font_bytes_))) {
                    return;
                }
                font_bytes_.clear();
            }
        }

        atom::render::Renderer2D renderer_{};
        std::vector<std::byte> font_bytes_{};
        atom::render::Font* font_ = nullptr;
};

class UiPage : public atom::Screen {
    public:
        explicit UiPage(std::shared_ptr<UiSurface> surface) : surface_(std::move(surface)) {}

        auto Render(atom::render::IRenderDevice& device) -> void override { surface_->Draw(device, ui_); }
        auto Update(float) -> void override { ui_.Update(); }

    protected:
        [[nodiscard]] auto HandleUiEvent(const atom::window::IEvent& event) -> bool { return ui_.HandleEvent(event); }
        atom::ui::UiTree ui_{};

    private:
        std::shared_ptr<UiSurface> surface_;
};

class MenuPage final : public UiPage {
    public:
        explicit MenuPage(std::shared_ptr<UiSurface> surface) : UiPage(std::move(surface)) {
            LayoutStyle root{};
            root.flex_direction = atom::layout::FlexDirection::Column;
            root.justify_content = atom::layout::Justify::Center;
            root.align_items = atom::layout::Align::Center;
            ui_.SetLayoutStyle(ui_.Root(), root);
            ui_.SetVisualStyle(ui_.Root(), Fill(Color{18, 24, 42, 255}));

            const auto card = ui_.CreatePanel(ui_.Root(), CardStyle(460.0f, 386.0f), Fill(Color{31, 42, 70, 255}, true));
            auto title = Box(408.0f, 52.0f);
            const auto title_node = ui_.CreateLabel(card, "Atom Runtime UI", title);
            auto title_visual = atom::ui::VisualStyle{};
            title_visual.text = Color{190, 218, 255, 255};
            title_visual.text_size = 31.0f;
            ui_.SetVisualStyle(title_node, title_visual);

            const auto button_style = Box(408.0f, 58.0f);
            static_cast<void>(ui_.CreateButton(card, "Start game", button_style, ButtonVisual(), [] {
                atom::ScreenManager::GetInstance().SwitchScreen("game");
            }));
            static_cast<void>(ui_.CreateButton(card, "Settings", button_style, ButtonVisual(), [] {
                atom::ScreenManager::GetInstance().SwitchScreen("settings");
            }));
            static_cast<void>(ui_.CreateButton(card, "Quit", button_style, ButtonVisual(), [] {
                atom::runtime::Application::GetInstance().RequestStop();
            }));
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override { return HandleUiEvent(event); }
};

class SettingsPage final : public UiPage {
    public:
        explicit SettingsPage(std::shared_ptr<UiSurface> surface) : UiPage(std::move(surface)) {
            LayoutStyle root{};
            root.flex_direction = atom::layout::FlexDirection::Column;
            root.justify_content = atom::layout::Justify::Center;
            root.align_items = atom::layout::Align::Center;
            ui_.SetLayoutStyle(ui_.Root(), root);
            ui_.SetVisualStyle(ui_.Root(), Fill(Color{23, 29, 48, 255}));
            const auto card = ui_.CreatePanel(ui_.Root(), CardStyle(500.0f, 320.0f), Fill(Color{42, 51, 78, 255}, true));

            auto heading = Box(448.0f, 52.0f);
            const auto heading_node = ui_.CreateLabel(card, "Settings", heading);
            auto heading_visual = atom::ui::VisualStyle{};
            heading_visual.text = Color{208, 225, 255, 255};
            heading_visual.text_size = 30.0f;
            ui_.SetVisualStyle(heading_node, heading_visual);

            auto description = Box(448.0f, 64.0f);
            static_cast<void>(ui_.CreateLabel(card, "This page is a normal Runtime screen, not a UI-owned page stack.", description));
            static_cast<void>(ui_.CreateButton(card, "Back to menu", Box(448.0f, 58.0f), ButtonVisual(), [] {
                atom::ScreenManager::GetInstance().SwitchScreen("menu");
            }));
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (HandleUiEvent(event))
                return true;
            if (event.type == atom::window::EventType::KeyPressed &&
                std::get<atom::window::KeyEvent>(event.data).key == atom::input::Key::Escape) {
                atom::ScreenManager::GetInstance().SwitchScreen("menu");
                return true;
            }
            return false;
        }
};

class GamePage final : public UiPage {
    public:
        explicit GamePage(std::shared_ptr<UiSurface> surface) : UiPage(std::move(surface)) {
            ui_.SetVisualStyle(ui_.Root(), Fill(Color{20, 53, 67, 255}));
            auto hud_layout = CardStyle(316.0f, 162.0f);
            hud_layout.position_type = atom::layout::PositionType::Absolute;
            hud_layout.position.left = atom::layout::Length::Points(24.0f);
            hud_layout.position.top = atom::layout::Length::Points(22.0f);
            const auto hud = ui_.CreatePanel(ui_.Root(), hud_layout, Fill(Color{12, 22, 38, 214}, true));
            static_cast<void>(ui_.CreateLabel(hud, "Demo game world", Box(264.0f, 32.0f)));
            static_cast<void>(ui_.CreateLabel(hud, "Runtime keeps this screen below pause.", Box(264.0f, 32.0f)));
            static_cast<void>(ui_.CreateButton(hud, "Pause", Box(264.0f, 48.0f), ButtonVisual(), [] {
                atom::ScreenManager::GetInstance().PushScreen("pause");
            }));
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            if (HandleUiEvent(event))
                return true;
            if (event.type == atom::window::EventType::KeyPressed &&
                std::get<atom::window::KeyEvent>(event.data).key == atom::input::Key::Escape) {
                atom::ScreenManager::GetInstance().PushScreen("pause");
                return true;
            }
            return false;
        }
};

class PausePage final : public UiPage {
    public:
        explicit PausePage(std::shared_ptr<UiSurface> surface) : UiPage(std::move(surface)) {
            LayoutStyle root{};
            root.flex_direction = atom::layout::FlexDirection::Column;
            root.justify_content = atom::layout::Justify::Center;
            root.align_items = atom::layout::Align::Center;
            ui_.SetLayoutStyle(ui_.Root(), root);
            ui_.SetVisualStyle(ui_.Root(), Fill(Color{0, 0, 0, 145}));
            ui_.SetModal(true);
            const auto card = ui_.CreatePanel(ui_.Root(), CardStyle(430.0f, 276.0f), Fill(Color{39, 45, 68, 255}, true));
            auto heading = Box(378.0f, 56.0f);
            const auto heading_node = ui_.CreateLabel(card, "Paused", heading);
            auto heading_visual = atom::ui::VisualStyle{};
            heading_visual.text = Color{235, 239, 255, 255};
            heading_visual.text_size = 32.0f;
            ui_.SetVisualStyle(heading_node, heading_visual);
            static_cast<void>(ui_.CreateButton(card, "Resume", Box(378.0f, 58.0f), ButtonVisual(), [] {
                atom::ScreenManager::GetInstance().PopScreen();
            }));
            static_cast<void>(ui_.CreateButton(card, "Main menu", Box(378.0f, 58.0f), ButtonVisual(), [] {
                atom::ScreenManager::GetInstance().SwitchScreen("menu");
            }));
        }

        auto HandleEvent(const atom::window::IEvent& event) -> bool override {
            // A modal overlay consumes every pointer/keyboard event that it
            // does not handle itself; ScreenManager therefore never lets a
            // paused game receive clicks or gameplay shortcuts.
            if (event.type == atom::window::EventType::KeyPressed &&
                std::get<atom::window::KeyEvent>(event.data).key == atom::input::Key::Escape) {
                static_cast<void>(HandleUiEvent(event)); // claim this key frame before pop
                atom::ScreenManager::GetInstance().PopScreen();
                return true;
            }
            return HandleUiEvent(event);
        }
};

} // namespace

auto main() -> int {
    auto& application = atom::runtime::Application::GetInstance();
    if (const auto result = application.Initialize("Atom UI Runtime Demo", {1280.0f, 720.0f}); !result)
        return 1;

    auto surface = std::make_shared<UiSurface>();
    auto surface_shutdown = application.AddShutdownObserver([surface] { surface->Shutdown(); });
    auto& screens = application.GetScreenManager();
    screens.LoadScreen("menu", std::make_unique<MenuPage>(surface));
    screens.LoadScreen("settings", std::make_unique<SettingsPage>(surface));
    screens.LoadScreen("game", std::make_unique<GamePage>(surface));
    screens.LoadScreen("pause", std::make_unique<PausePage>(surface));
    screens.SwitchScreen("menu");
    application.Run();
    return 0;
}
