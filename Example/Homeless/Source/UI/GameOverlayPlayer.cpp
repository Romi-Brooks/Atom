#include "GameOverlay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
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
        std::vector<char> font_bytes;
};

namespace {

constexpr auto ink = atom::color::Color{14, 27, 33, 255};
constexpr auto highlight = atom::color::Color{54, 82, 91, 255};
constexpr auto cream = atom::color::Color{239, 235, 223, 255};

auto ReadSystemFont(const char* filename) -> std::vector<char> {
    const char* directory = std::getenv("WINDIR");
    const std::filesystem::path root = directory ? directory : "C:/Windows";
    std::ifstream stream(root / "Fonts" / filename, std::ios::binary);
    if (!stream)
        return {};
    return {std::istreambuf_iterator<char>(stream), {}};
}

auto LoadSystemFont(atom::render::Renderer2D& renderer, const char* filename) -> atom::render::Font* {
    const auto bytes = ReadSystemFont(filename);
    return bytes.empty() ? nullptr : renderer.LoadFontFromMemory(std::as_bytes(std::span(bytes)));
}

auto TrackFontName(const std::wstring& text) -> const char* {
    if (std::ranges::any_of(text, [](wchar_t c) {
            return (c >= 0x1100 && c <= 0x11ff) || (c >= 0x3130 && c <= 0x318f) || (c >= 0xac00 && c <= 0xd7af);
        }))
        return "malgun.ttf";
    if (std::ranges::any_of(text, [](wchar_t c) { return c >= 0x2e80 && c <= 0x9fff; }))
        return "msyh.ttc";
    return nullptr;
}

auto MakeCircularCover(const atom::image::DecodedImage& image) -> atom::image::DecodedImage {
    if (!image.IsValid())
        return {};

    const auto edge = std::min(image.width, image.height);
    const auto size = std::min(edge, 192u);
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
    TrackPresentation result{.title = std::move(title), .artist = std::move(artist)};
    if (const auto tags = atom::audio::AudioMetadataReader::Read(uri)) {
        if (!tags->title.empty())
            result.title = tags->title;
        if (!tags->artist.empty())
            result.artist = tags->artist;
        if (!tags->artworkData.empty()) {
            const auto image = atom::image::DecodeImageMemory(std::as_bytes(std::span(tags->artworkData)));
            result.cover = MakeCircularCover(image);
        }
    }
    if (const auto* font_name = TrackFontName(atom::Utf8ToWide(result.title + result.artist)))
        result.font_bytes = ReadSystemFont(font_name);
    return result;
}

} // namespace

auto GameOverlay::Load(const std::string& track_uri) -> bool {
    font_ = LoadSystemFont(renderer_, "segoeui.ttf");
    if (!font_)
        font_ = LoadSystemFont(renderer_, "arial.ttf");
    if (!font_)
        return false;

    if (track_uri.empty() || !music_.Load("phone", track_uri))
        return true;

    track_available_ = true;
    title_ = atom::PathToUtf8(atom::PathFromUtf8(track_uri.substr(track_uri.find_last_of('/') + 1)).stem());
    if (const auto separator = title_.find(" - "); separator != std::string::npos) {
        artist_ = title_.substr(0, separator);
        title_ = title_.substr(separator + 3);
    }
    prepared_track_ = std::make_shared<TrackPresentation>();
    auto prepare = [result = prepared_track_, track_uri, title = title_, artist = artist_]() mutable {
        *result = ReadTrackPresentation(track_uri, std::move(title), std::move(artist));
    };
    prepare_track_job_ =
        atom::job::JobSystem::GetInstance().ScheduleOrRun("Homeless: player artwork and tags", std::move(prepare));
    ApplyPreparedTrack();

    return true;
}

auto GameOverlay::ApplyPreparedTrack() -> void {
    if (!prepared_track_ || (prepare_track_job_.IsValid() && !prepare_track_job_.IsComplete()))
        return;
    auto prepared = std::move(prepared_track_);
    prepare_track_job_ = {};
    if (!prepared->title.empty())
        title_ = std::move(prepared->title);
    if (!prepared->artist.empty())
        artist_ = std::move(prepared->artist);
    if (prepared->cover.IsValid())
        cover_ = renderer_.CreateTexture(prepared->cover.width, prepared->cover.height, prepared->cover.rgba.data());
    if (!prepared->font_bytes.empty())
        track_font_ = renderer_.LoadFontFromMemory(std::as_bytes(std::span(prepared->font_bytes)));
}

auto GameOverlay::Update(float seconds) -> void {
    ApplyPreparedTrack();
    playing_ = track_available_ && music_.GetState("phone") == atom::audio::AudioSourceState::Playing;
    if (playing_ && std::isfinite(seconds) && seconds > 0.0f)
        cover_angle_ = std::fmod(cover_angle_ + std::min(seconds, 0.1f) * 1.6f, 2 * std::numbers::pi_v<float>);
}

auto GameOverlay::ToggleTrack() -> void {
    if (!track_available_)
        return;
    if (music_.GetState("phone") == atom::audio::AudioSourceState::Playing)
        music_.Pause("phone");
    else
        music_.Play("phone");
}

auto GameOverlay::PlayerScreen() const -> atom::algo::Rect {
    return {ui_size_.GetX() - 161, ui_size_.GetY() - 80, 144, 47};
}

auto GameOverlay::PlayButton() const -> atom::algo::Rect {
    const auto screen = PlayerScreen();
    return {screen.x + 54, screen.y + 52, 36, 15};
}

auto GameOverlay::HandlePlayerClick() -> bool {
    if (!PlayerScreen().Contains(mouse_) && !PlayButton().Contains(mouse_))
        return false;
    ToggleTrack();
    return true;
}

auto GameOverlay::DrawPlayer() -> void {
    const auto screen = PlayerScreen();
    const auto play = PlayButton();
    const auto x = screen.x;
    const auto y = screen.y;
    constexpr auto bezel = atom::color::Color{164, 180, 194, 255};
    constexpr auto bevel = atom::color::Color{212, 227, 233, 255};
    constexpr auto case_edge = atom::color::Color{54, 65, 89, 255};
    constexpr auto passive = atom::color::Color{108, 120, 136, 255};
    constexpr auto active = atom::color::Color{207, 243, 248, 255};
    constexpr auto screen_tint = atom::color::Color{16, 30, 52, 255};
    const bool hovered = track_available_ && (screen.Contains(mouse_) || play.Contains(mouse_));

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
    if (cover_) {
        renderer_.DrawTexture(*cover_, disc, atom::color::Color::White(), nullptr, false, false,
                              atom::render::Filter2D::Linear, cover_angle_);
    } else {
        renderer_.DrawCircle(cx, cy, 12.4f * ui_scale_, {71, 104, 125, 255}, 48);
        renderer_.DrawCircle(cx, cy, 4.3f * ui_scale_, {168, 206, 218, 255}, 24);
    }

    renderer_.PushClip(ToPixels({x + 51, y + 4, 90, 39}));
    auto& track_face = track_font_ ? *track_font_ : *font_;
    Label(track_face, title_, x + 52, y + 9, 11, cream);
    Label(track_face, artist_, x + 53, y + 26, 9, {155, 201, 219, 255});
    renderer_.PopClip();

    const auto transport = [&](const atom::algo::Rect& bounds, atom::color::Color background) {
        Fill(bounds, case_edge);
        Fill({bounds.x + 1, bounds.y + 1, bounds.width - 2, bounds.height - 2}, background);
    };
    transport({x + 17, y + 53, 31, 13}, {136, 153, 170, 255});
    transport({x + 96, y + 53, 31, 13}, {136, 153, 170, 255});
    transport(play, hovered ? highlight : atom::color::Color{47, 69, 93, 255});

    const float icon_y = y + 59.5f;
    const auto arrow = [&](float center, int direction, atom::color::Color color) {
        for (int column = 0; column < 5; ++column) {
            const float half_height = 3.6f - column * 0.65f;
            Fill({center + direction * (column - 2), icon_y - half_height, 1.15f, 2 * half_height}, color);
        }
    };
    arrow(x + 33, -1, passive);
    Fill({x + 28, icon_y - 3.5f, 1.4f, 7}, passive);
    arrow(x + 111, 1, passive);
    Fill({x + 115, icon_y - 3.5f, 1.4f, 7}, passive);
    if (playing_) {
        Fill({x + 68, icon_y - 3.5f, 2.6f, 7}, active);
        Fill({x + 73, icon_y - 3.5f, 2.6f, 7}, active);
    } else {
        arrow(x + 72, 1, track_available_ ? active : passive);
    }
}

} // namespace homeless
