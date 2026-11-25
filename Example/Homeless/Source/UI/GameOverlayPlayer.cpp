#include "GameOverlay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include <Concurrency/JobSystem.hpp>
#include <Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Utilities/Utf8/Utf8.hpp>

namespace homeless {

struct TrackPresentation {
        std::string title;
        std::string artist;
        atom::image::DecodedImage cover;
};

namespace {

constexpr auto ink = atom::color::Color{14, 27, 33, 255};
constexpr auto highlight = atom::color::Color{54, 82, 91, 255};
constexpr auto cream = atom::color::Color{239, 235, 223, 255};
enum class TrackScript { Latin, Chinese, Korean };

auto ReadSystemFont(const char* filename) -> std::vector<char> {
    const char* directory = std::getenv("WINDIR");
    const std::filesystem::path root = directory ? directory : "C:/Windows";
    std::ifstream stream(root / "Fonts" / filename, std::ios::binary | std::ios::ate);
    const auto size = stream.tellg();
    if (size <= 0)
        return {};
    std::vector<char> bytes(static_cast<std::size_t>(size));
    stream.seekg(0);
    if (!stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        return {};
    return bytes;
}

auto DisplayLine(std::string_view text) -> std::string {
    const auto line = text.substr(0, text.find_first_of("\r\n"));
    auto end = std::min<std::size_t>(line.size(), 96);
    while (end < line.size() && end > 0 && (static_cast<unsigned char>(line[end]) & 0xC0) == 0x80)
        --end;
    return atom::ReplaceInvalidUtf8(std::string(line.substr(0, end)));
}

auto FindTrackScript(const std::wstring& text) -> TrackScript {
    if (std::ranges::any_of(text, [](wchar_t c) {
            return (c >= 0x1100 && c <= 0x11ff) || (c >= 0x3130 && c <= 0x318f) || (c >= 0xac00 && c <= 0xd7af);
        }))
        return TrackScript::Korean;
    if (std::ranges::any_of(text, [](wchar_t c) { return c >= 0x2e80 && c <= 0x9fff; }))
        return TrackScript::Chinese;
    return TrackScript::Latin;
}

auto MakeCircularCover(const atom::image::DecodedImage& image) -> atom::image::DecodedImage {
    if (!image.IsValid())
        return {};

    const auto edge = std::min(image.width, image.height);
    constexpr auto size = 192u;
    const auto left = (image.width - edge) / 2;
    const auto top = (image.height - edge) / 2;
    const float radius = static_cast<float>(size) * 0.5f;
    std::vector<std::uint8_t> rgba(size * size * 4);
    for (std::uint32_t y = 0; y < size; ++y) {
        const auto source_y = top + y * edge / size;
        for (std::uint32_t x = 0; x < size; ++x) {
            const auto source_x = left + x * edge / size;
            const auto source = (static_cast<std::size_t>(source_y) * image.width + source_x) * 4;
            const auto destination = (y * size + x) * 4;
            std::copy_n(image.rgba.data() + source, 3, rgba.data() + destination);
            const float distance = std::hypot(x + 0.5f - radius, y + 0.5f - radius);
            const float coverage = std::clamp(radius - distance, 0.0f, 1.0f);
            rgba[destination + 3] = static_cast<std::uint8_t>(image.rgba[source + 3] * coverage);
        }
    }
    return {size, size, std::move(rgba)};
}

auto ReadTrackPresentation(const std::string& uri, std::string title, std::string artist) -> TrackPresentation {
    TrackPresentation result{.title = DisplayLine(title), .artist = DisplayLine(artist)};
    if (const auto tags = atom::audio::AudioMetadataReader::Read(uri)) {
        if (!tags->title.empty())
            result.title = DisplayLine(tags->title);
        if (!tags->artist.empty())
            result.artist = DisplayLine(tags->artist);
        if (!tags->artworkData.empty()) {
            const auto image = atom::image::DecodeImageMemory(std::as_bytes(std::span(tags->artworkData)));
            result.cover = MakeCircularCover(image);
        }
    }
    return result;
}

} // namespace

auto GameOverlay::ReadFonts() -> OverlayFonts {
    auto latin = ReadSystemFont("segoeui.ttf");
    if (latin.empty())
        latin = ReadSystemFont("arial.ttf");
    return {std::move(latin), ReadSystemFont("msyh.ttc"), ReadSystemFont("malgun.ttf")};
}

auto GameOverlay::Load(const OverlayFonts& fonts, const std::filesystem::path& music_directory) -> bool {
    const auto load = [this](const std::vector<char>& bytes) {
        return bytes.empty() ? nullptr : renderer_.LoadFontFromMemory(std::as_bytes(std::span(bytes)));
    };
    font_ = load(fonts.latin);
    if (!font_)
        return false;
    chinese_font_ = load(fonts.chinese);
    korean_font_ = load(fonts.korean);
    playlist_.Scan(music_directory);
    return true;
}

auto GameOverlay::UpdateTrackDisplay() -> void {
    const auto* track = playlist_.Selected();
    const std::string_view uri = track ? std::string_view{track->uri} : std::string_view{};
    bool text_changed = false;
    if (selected_uri_ != uri) {
        selected_uri_ = uri;
        title_ = track ? DisplayLine(track->title) : "No track";
        artist_ = track ? DisplayLine(track->artist) : "";
        show_cover_ = false;
        cover_angle_ = 0.0f;
        text_changed = true;
    }
    if (prepare_track_job_.IsValid() && prepare_track_job_.IsComplete()) {
        prepare_track_job_ = {};
        auto prepared = std::move(prepared_track_);
        if (track && artwork_uri_ == uri) {
            if (!prepared->title.empty())
                title_ = std::move(prepared->title);
            if (!prepared->artist.empty())
                artist_ = std::move(prepared->artist);
            if (prepared->cover.IsValid()) {
                const auto& cover = prepared->cover;
                if (cover_)
                    renderer_.UpdateTexture(*cover_, cover.rgba.data());
                else
                    cover_ = renderer_.CreateTexture(cover.width, cover.height, cover.rgba.data());
                show_cover_ = cover_ != nullptr;
            }
            text_changed = true;
        }
    }
    if (track && artwork_uri_ != uri && !prepare_track_job_.IsValid()) {
        artwork_uri_ = uri;
        prepared_track_ = std::make_shared<TrackPresentation>();
        prepare_track_job_ = atom::job::JobSystem::GetInstance().Schedule(
            "Homeless: music artwork and tags",
            [result = prepared_track_, track = *track]() mutable {
                *result = ReadTrackPresentation(track.uri, std::move(track.title), std::move(track.artist));
            },
            atom::job::Priority::Low);
    }
    if (text_changed) {
        const auto script = FindTrackScript(atom::Utf8ToWide(title_ + artist_));
        track_font_ = script == TrackScript::Chinese  ? chinese_font_
                      : script == TrackScript::Korean ? korean_font_
                                                      : nullptr;
    }
}

auto GameOverlay::Update(float seconds) -> void {
    playlist_.Update(mixer_.GetEffectiveMusicVolume());
    UpdateTrackDisplay();
    if (playlist_.IsPlaying() && std::isfinite(seconds) && seconds > 0.0f)
        cover_angle_ = std::fmod(cover_angle_ + std::min(seconds, 0.1f) * 1.6f, 2 * std::numbers::pi_v<float>);
}

auto GameOverlay::PlayerScreen() const -> atom::algo::Rect {
    return {ui_size_.GetX() - 161, ui_size_.GetY() - 80, 144, 47};
}

auto GameOverlay::PlayButton() const -> atom::algo::Rect {
    const auto screen = PlayerScreen();
    return {screen.x + 54, screen.y + 52, 36, 15};
}

auto GameOverlay::SkipButton(int direction) const -> atom::algo::Rect {
    const auto screen = PlayerScreen();
    return {screen.x + (direction < 0 ? 17.0f : 96.0f), screen.y + 53, 31, 13};
}

auto GameOverlay::HandlePlayerClick() -> bool {
    for (const int direction : {-1, 1}) {
        if (SkipButton(direction).Contains(mouse_)) {
            if (playlist_.CanMove())
                playlist_.Move(direction);
            return true;
        }
    }
    if (!PlayerScreen().Contains(mouse_) && !PlayButton().Contains(mouse_))
        return false;
    playlist_.Toggle();
    return true;
}

auto GameOverlay::DrawPlayer() -> void {
    const auto screen = PlayerScreen();
    const auto play = PlayButton();
    const auto previous = SkipButton(-1);
    const auto next = SkipButton(1);
    const auto x = screen.x;
    const auto y = screen.y;
    constexpr auto bezel = atom::color::Color{164, 180, 194, 255};
    constexpr auto bevel = atom::color::Color{212, 227, 233, 255};
    constexpr auto case_edge = atom::color::Color{54, 65, 89, 255};
    constexpr auto passive = atom::color::Color{108, 120, 136, 255};
    constexpr auto active = atom::color::Color{207, 243, 248, 255};
    constexpr auto screen_tint = atom::color::Color{16, 30, 52, 255};
    const bool track_available = playlist_.Selected() != nullptr;
    const bool hovered = track_available && (screen.Contains(mouse_) || play.Contains(mouse_));

    Fill({x - 4, y - 5, 157, 80}, {10, 13, 31, 151});
    Fill({x - 7, y - 5, 158, 76}, case_edge);
    Fill({x - 5, y - 7, 154, 80}, case_edge);
    Fill({x - 5, y - 5, 154, 75}, bezel);
    Fill({x - 3, y - 5, 150, 1}, bevel);
    Fill({x - 3, y + 70, 150, 2}, {107, 122, 140, 255});

    Fill({x - 2, y - 2, screen.width + 4, screen.height + 4}, ink);
    Fill(screen, hovered ? atom::color::Color{25, 46, 70, 255} : screen_tint);
    Fill({x + 2, y + 1, screen.width - 4, 1}, {86, 137, 160, 255});

    const auto disc = ToPixels({x + 5, y + 4, 39, 39});
    const float cx = disc.x + disc.width * 0.5f;
    const float cy = disc.y + disc.height * 0.5f;
    renderer_.DrawCircle(cx, cy, 20.5f * ui_scale_, {94, 133, 153, 255}, 48);
    renderer_.DrawCircle(cx, cy, 19.6f * ui_scale_, {27, 42, 62, 255}, 48);
    if (show_cover_) {
        renderer_.DrawTexture(*cover_, disc, atom::color::Color::White(), nullptr, false, false,
                              atom::render::Filter2D::Linear, cover_angle_);
    } else {
        renderer_.DrawCircle(cx, cy, 12.4f * ui_scale_, {71, 104, 125, 255}, 48);
        renderer_.DrawCircle(cx, cy, 4.3f * ui_scale_, {168, 206, 218, 255}, 24);
    }

    renderer_.PushClip(ToPixels({x + 51, y + 4, 90, 39}));
    auto& track_face = track_font_ ? *track_font_ : *font_;
    Label(track_face, playlist_.IsScanning() ? "Reading Music" : title_, x + 52, y + 9, 11, cream);
    Label(track_face, artist_, x + 53, y + 26, 9, {155, 201, 219, 255});
    renderer_.PopClip();

    const auto transport = [&](const atom::algo::Rect& bounds, atom::color::Color background) {
        Fill(bounds, case_edge);
        Fill({bounds.x + 1, bounds.y + 1, bounds.width - 2, bounds.height - 2}, background);
    };
    constexpr auto button_idle = atom::color::Color{136, 153, 170, 255};
    const bool can_skip = playlist_.CanMove();
    transport(previous, can_skip && previous.Contains(mouse_) ? highlight : button_idle);
    transport(next, can_skip && next.Contains(mouse_) ? highlight : button_idle);
    transport(play, hovered ? highlight : atom::color::Color{47, 69, 93, 255});

    const float icon_y = y + 59.5f;
    const auto arrow = [&](float center, int direction, atom::color::Color color) {
        for (int column = 0; column < 5; ++column) {
            const float half_height = 3.6f - column * 0.65f;
            Fill({center + direction * (column - 2), icon_y - half_height, 1.15f, 2 * half_height}, color);
        }
    };
    const auto skip_color = can_skip ? ink : passive;
    arrow(x + 33, -1, skip_color);
    Fill({x + 28, icon_y - 3.5f, 1.4f, 7}, skip_color);
    arrow(x + 111, 1, skip_color);
    Fill({x + 115, icon_y - 3.5f, 1.4f, 7}, skip_color);
    if (playlist_.WantsPlayback()) {
        Fill({x + 68, icon_y - 3.5f, 2.6f, 7}, active);
        Fill({x + 73, icon_y - 3.5f, 2.6f, 7}, active);
    } else {
        arrow(x + 72, 1, track_available ? active : passive);
    }
}

} // namespace homeless
