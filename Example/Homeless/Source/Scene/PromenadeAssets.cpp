#include "PromenadeScreen.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include <Concurrency/JobSystem.hpp>
#include <Log/LogSystem.hpp>
#include <Media/Image/ImageBatchDecoder.hpp>
#include <Render/Scene2D/Scene2DRenderer.hpp>

#include "PromenadeConfig.hpp"

namespace homeless {

namespace {

constexpr std::array scene_textures{waterfront_uri, street_uri, walker_uri, pedestrian_uri,
                                    car_uri,        snow_uri,   breath_uri};

struct WaterMask {
        unsigned width = 0;
        std::vector<atom::algo::Rect> spans;
};

struct SceneAssets {
        std::array<atom::image::DecodedImage, scene_textures.size()> textures;
        WaterMask water;
        OverlayFonts interface_fonts;
};

auto ReadImage(std::string_view uri) -> atom::image::DecodedImage {
    return atom::image::ImageBatchDecoder::Decode(std::string(uri)).value_or(atom::image::DecodedImage{});
}

auto ReadWaterMask() -> WaterMask {
    const auto mask = ReadImage("res://Textures/WaterfrontMask.png");
    if (!mask.IsValid() || mask.height != scene_height)
        return {};

    WaterMask result{.width = mask.width};
    // Erode each span by the maximum displacement so shifted samples stay in water.
    constexpr int margin = 2;
    const auto mask_width = static_cast<int>(mask.width);
    for (int y = 0; y < scene_height; ++y) {
        const auto wet = [&](int x) { return mask.rgba[(y * mask_width + x) * 4] >= 128; };
        for (int x = 0; x < mask_width;) {
            if (!wet(x)) {
                ++x;
                continue;
            }
            const int start = x++;
            while (x < mask_width && wet(x))
                ++x;
            if (x - start > 2 * margin)
                result.spans.emplace_back(static_cast<float>(start + margin), static_cast<float>(y),
                                          static_cast<float>(x - start - 2 * margin), 1.0f);
        }
    }
    return result;
}

} // namespace

auto PromenadeScreen::Create(atom::render::IRenderDevice& device, const std::filesystem::path& directory,
                             unsigned frame_limit, bool& render_failed) -> std::unique_ptr<PromenadeScreen> {
    auto world = atom::ecs::EntityWorld::Create();
    if (!world) {
        LOG_ERROR(atom::log::core::Screen, world.error().Message());
        return {};
    }
    auto screen = std::unique_ptr<PromenadeScreen>(
        new PromenadeScreen(std::move(*world), frame_limit, render_failed, atom::random::Rng::FromEntropy()));
    auto& jobs = atom::job::JobSystem::GetInstance();
    auto assets = std::make_shared<SceneAssets>();
    std::vector<atom::job::JobHandle> handles;
    handles.reserve(scene_textures.size() + 2);
    // Each job writes a separate result and depends only on the mounted filesystems.
    for (std::size_t i = 0; i < scene_textures.size(); ++i) {
        const auto uri = scene_textures[i];
        handles.push_back(jobs.ScheduleOrRun("Homeless texture: " + std::string(uri.substr(uri.find_last_of('/') + 1)),
                                             [assets, i, uri] { assets->textures[i] = ReadImage(uri); }));
    }
    handles.push_back(
        jobs.ScheduleOrRun("Homeless: water reflection mask", [assets] { assets->water = ReadWaterMask(); }));
    handles.push_back(jobs.ScheduleOrRun("Homeless: interface fonts",
                                         [assets] { assets->interface_fonts = GameOverlay::ReadFonts(); }));

    const bool renderer_ready = screen->renderer_.Initialize(device, directory / "Shaders");
    // Only required startup resources are joined here, before the frame loop.
    jobs.WaitAll(handles);
    const bool overlay_ready = renderer_ready && screen->overlay_.Load(assets->interface_fonts, directory / "Music");
    if (!renderer_ready || !overlay_ready) {
        LOG_ERROR(atom::log::render::Renderer2D, "Cannot initialize Homeless Renderer2D or interface font");
        return {};
    }

    // Upload on the render thread. Keep these handles alive while the scene
    // resolver adopts the same cached textures.
    std::array<atom::render::resources::TextureHandle, scene_textures.size()> preloaded;
    for (std::size_t i = 0; i < scene_textures.size(); ++i)
        preloaded[i] = screen->cache_.AcquireFromDecodedImage(scene_textures[i], assets->textures[i]);

    if (!std::ranges::all_of(preloaded, &atom::render::resources::TextureHandle::IsValid)) {
        LOG_ERROR(atom::log::asset::Resource, "Cannot load Homeless scene textures");
        return {};
    }
    screen->waterfront_ = screen->textures_.Resolve(waterfront_uri);
    screen->street_ = screen->textures_.Resolve(street_uri);
    const auto* walker = screen->textures_.Resolve(walker_uri);
    const auto* pedestrians = screen->textures_.Resolve(pedestrian_uri);
    const auto* traffic = screen->textures_.Resolve(car_uri);
    if (screen->waterfront_)
        screen->world_width_ = static_cast<float>(screen->waterfront_->GetWidth());
    screen->water_spans_ = std::move(assets->water.spans);
    // Both areas share the same road, traffic simulation and camera extent.
    if (!screen->waterfront_ || !screen->street_ || !walker || !pedestrians || !traffic ||
        screen->world_width_ < scene_width || screen->waterfront_->GetHeight() != scene_height ||
        screen->street_->GetHeight() != scene_height ||
        screen->street_->GetWidth() != screen->waterfront_->GetWidth() || walker->GetWidth() != 256 ||
        walker->GetHeight() != 46 || traffic->GetWidth() != 432 || traffic->GetHeight() != 32 ||
        pedestrians->GetWidth() != Pedestrians::frame_width * Pedestrians::frame_count ||
        pedestrians->GetHeight() != Pedestrians::frame_height * Pedestrians::outfit_count * Pedestrians::color_count ||
        assets->water.width != screen->waterfront_->GetWidth() || screen->water_spans_.empty() ||
        !screen->SetUpWorld()) {
        LOG_ERROR(atom::log::asset::Resource,
                  "Cannot load Homeless scenes. Check the texture dimensions and water mask.");
        return {};
    }
    assets.reset();

    // Start with snowfall already distributed down the screen.
    for (int step = 0; step < 600; ++step) {
        (void)atom::render::scene2d::Scene2DRenderer::SnapshotTransforms(*screen->world_);
        screen->UpdateWeather(1.0f / 60.0f);
        screen->StepTraffic(1.0f / 60.0f);
    }
    return screen;
}

} // namespace homeless
