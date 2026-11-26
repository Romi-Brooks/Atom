#include "GameOverlay.hpp"

#include <algorithm>

#include <Atom/Platform/Window/RenderWindow.hpp>

#include "Scene/PromenadeConfig.hpp"

namespace homeless {

namespace {

constexpr auto cream = atom::color::Color{239, 235, 223, 255};

} // namespace

GameOverlay::GameOverlay(atom::render::Renderer2D& renderer, atom::AudioMixer& mixer)
    : renderer_(renderer), mixer_(mixer), playlist_(mixer) {}

auto GameOverlay::ToggleEffects() -> void {
    const auto volume = mixer_.GetSFXVolume();
    if (volume > 0)
        last_sfx_ = volume;
    mixer_.SetSFXVolume(volume > 0 ? 0.0f : last_sfx_);
}

auto GameOverlay::ToUi(float x, float y) const -> atom::algo::Vec2 {
    const auto* window = atom::RenderWindow::GetInstance().GetIWindow();
    const auto size = window ? window->GetCoordinateSize() : atom::algo::Vec2{};
    if (size.GetX() <= 0 || size.GetY() <= 0)
        return {};
    return {x * ui_size_.GetX() / size.GetX(), y * ui_size_.GetY() / size.GetY()};
}

auto GameOverlay::ToPixels(const atom::algo::Rect& rect) const -> atom::algo::Rect {
    return {rect.x * ui_scale_, rect.y * ui_scale_, rect.width * ui_scale_, rect.height * ui_scale_};
}

auto GameOverlay::Fill(const atom::algo::Rect& rect, atom::color::Color color) -> void {
    renderer_.DrawRect(ToPixels(rect), color);
}

auto GameOverlay::Outline(const atom::algo::Rect& rect, atom::color::Color color) -> void {
    renderer_.DrawRectOutline(ToPixels(rect), color, ui_scale_);
}

auto GameOverlay::Label(atom::render::Font& font, std::string_view text, float x, float y, float size,
                        atom::color::Color color) -> void {
    renderer_.DrawText(font, text, x * ui_scale_, y * ui_scale_, color, size * ui_scale_);
}

auto GameOverlay::Text(std::string_view text, float x, float y, float size) -> void {
    Label(*font_, text, x, y, size, cream);
}

auto GameOverlay::DrawInteractionPrompt(InteractionPrompt prompt) -> void {
    const float y = ui_size_.GetY() - 38.0f;
    const atom::algo::Rect panel{14, y, 125, 24};
    Fill(panel, {19, 17, 37, 240});
    Outline(panel, {116, 113, 156, 255});
    float label_x = 22.0f;
    if (!prompt.checked) {
        Fill({21, y + 5, 14, 14}, {73, 63, 102, 255});
        Text("E", 25, y + 5, 10);
        label_x = 42.0f;
    }
    if (chinese_font_) {
        const auto label = prompt.checked ? "入口暂未开放" : prompt.name;
        Label(*chinese_font_, label, label_x, y + 4, 11, cream);
    } else {
        Text(prompt.checked ? "Entrance unavailable" : prompt.fallback_name, label_x, y + 5, 10);
    }
}

auto GameOverlay::Draw(atom::render::IRenderDevice& device, InteractionPrompt prompt) -> bool {
    const auto output = device.GetOutputSize();
    const auto zoom = std::min(output.GetX() / scene_width, output.GetY() / scene_height);
    if (zoom <= 0)
        return true;
    ui_size_ = {output.GetX() / zoom, output.GetY() / zoom};
    ui_scale_ = zoom;
    if (!renderer_.BeginFrame())
        return false;
    DrawPlayer();
    if (IsMenuOpen())
        DrawMenu();
    else if (!prompt.name.empty())
        DrawInteractionPrompt(prompt);
    return renderer_.EndFrame();
}

} // namespace homeless
