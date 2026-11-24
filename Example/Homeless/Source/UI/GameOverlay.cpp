#include "GameOverlay.hpp"

#include <algorithm>

#include <Window/RenderWindow.hpp>

#include "Scene/PromenadeConfig.hpp"

namespace homeless {

namespace {

constexpr auto cream = atom::color::Color{239, 235, 223, 255};

} // namespace

GameOverlay::GameOverlay(atom::render::Renderer2D& renderer, atom::AudioMixer& mixer, atom::MusicPlayer& music,
                         atom::SFXPlayer& effects)
    : renderer_(renderer), mixer_(mixer), music_(music), effects_(effects) {}

auto GameOverlay::ApplyAudioLevels() -> void {
    music_.SetVolume("phone", mixer_.GetEffectiveMusicVolume());
    music_.SetVolume("waterfront", mixer_.GetEffectiveSFXVolume() * 0.34f);
    effects_.SetVolume("passing-car", mixer_.GetEffectiveSFXVolume() * 0.18f);
}

auto GameOverlay::ToggleEffects() -> void {
    const auto volume = mixer_.GetSFXVolume();
    if (volume > 0)
        last_sfx_ = volume;
    mixer_.SetSFXVolume(volume > 0 ? 0.0f : last_sfx_);
    ApplyAudioLevels();
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

auto GameOverlay::Draw(atom::render::IRenderDevice& device) -> bool {
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
    return renderer_.EndFrame();
}

} // namespace homeless
