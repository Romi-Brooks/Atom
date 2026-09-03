/**
  * @file           : Renderer2DSmoke.cpp
  * @author         : Romi Brooks
  * @brief          : Renderer2D batching, clipping, atlas and camera smoke test.
  * @attention      : Text/font coverage is exercised by Example_MusicCard.
  * @date           : 2026/9/4
  Copyright (c) 2026 Romi Brooks, All rights reserved.
**/

// Renderer2D smoke scene (phase C M2): batched filled/outlined rects, circles,
// lines, a small texture atlas with sub-rect draws and tinting, scissor
// clipping, layers and camera pan/zoom.
//
// Renderer2D text and CJK glyph-atlas rendering are exercised by MusicCard;
// this focused scene keeps no external/system-font dependency.

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <Backend/SDLGPU/Device/SDLGPUBackend.hpp>
#include <Backend/SDLGPU/Device/SDLGPUDevice.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Render/Renderer2D/Renderer2D.hpp>

auto main() -> int {
    atom::backend::sdlgpu::SDLGPUBackend backend;
    if (!backend.Initialize("Atom Renderer2D (Phase C)", {1024.0f, 640.0f}))
        return 1;
    auto& device = static_cast<atom::backend::sdlgpu::SDLGPUDevice&>(backend.Device());

    atom::render::Renderer2D renderer;
    if (!renderer.Initialize(device, ATOM_SHADER_OUTPUT_DIR))
        return 2;

    // 2x2 tile atlas (each tile 64x64): top-left warm, top-right cool,
    // bottom-left green, bottom-right magenta.
    auto* atlas = renderer.CreateTexture(128, 128, nullptr);
    if (!atlas)
        return 5;
    std::vector<uint8_t> atlasPixels(static_cast<std::size_t>(128) * 128 * 4);
    const std::array<uint8_t, 16> tileColors{
        {200, 80, 80, 255, 80, 140, 220, 255, 80, 200, 110, 255, 220, 80, 200, 255}};
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const int tile = (x / 64) + (y / 64) * 2;
            auto* pixel = atlasPixels.data() + (static_cast<std::size_t>(y) * 128 + static_cast<std::size_t>(x)) * 4;
            pixel[0] = tileColors[tile * 4 + 0];
            pixel[1] = tileColors[tile * 4 + 1];
            pixel[2] = tileColors[tile * 4 + 2];
            pixel[3] = tileColors[tile * 4 + 3];
            if ((x / 8 + y / 8) % 2 == 0) {
                pixel[0] = static_cast<uint8_t>(pixel[0] * 3 / 4);
                pixel[1] = static_cast<uint8_t>(pixel[1] * 3 / 4);
                pixel[2] = static_cast<uint8_t>(pixel[2] * 3 / 4);
            }
        }
    }
    renderer.UpdateTexture(*atlas, atlasPixels.data());

    // Phase C M3: decode a real PNG file (CPU) then upload it as a texture
    // (GPU) — the two steps stay decoupled (Media/Image + Renderer2D).
    const std::string samplePath = std::string{ATOM_PROJECT_ROOT} + "/Example/Render/assets/sample.png";
    auto decoded = atom::image::DecodeImageFile(samplePath);
    auto* photo =
        decoded.IsValid() ? renderer.CreateTexture(decoded.width, decoded.height, decoded.rgba.data()) : nullptr;
    if (!photo)
        return 6;

    const double start = backend.Window().GetTimeSeconds();
    while (backend.Window().IsOpen() && backend.Window().GetTimeSeconds() - start < 12.0) {
        while (const auto event = backend.Window().PollEvent()) {
            if (event->type == atom::window::EventType::Closed)
                break;
        }
        if (!device.BeginFrame())
            continue;
        device.Clear(atom::render::Color{24, 28, 40});

        const auto size = device.GetOutputSize();
        const float w = size.GetX();
        const float h = size.GetY();
        const float time = static_cast<float>(backend.Window().GetTimeSeconds() - start);

        // Camera: center view on (w/2, h/2) with a slow pan/zoom wobble.
        const float zoom = 1.0f + 0.08f * std::sin(time * 0.6f);
        const float centerX = w * 0.5f + std::sin(time * 0.45f) * 24.0f;
        const float centerY = h * 0.5f + std::cos(time * 0.33f) * 18.0f;
        const float originX = centerX - w * 0.5f / zoom;
        const float originY = centerY - h * 0.5f / zoom;

        if (!renderer.BeginFrame(originX, originY, zoom))
            break;

        // Layer 0: faint grid lines across the whole view.
        renderer.PushLayer(0);
        const float step = 64.0f;
        for (float x = 0.0f; x <= w; x += step) {
            const float a = std::max(0.05f, 0.25f * std::sin(x * 0.01f + time) + 0.25f);
            renderer.DrawLine(x, 0.0f, x, h, atom::render::Color{60, 70, 90, static_cast<uint8_t>(a * 255.0f)});
        }
        for (float y = 0.0f; y <= h; y += step)
            renderer.DrawLine(0.0f, y, w, y, atom::render::Color{60, 70, 90, 60});
        renderer.PopLayer();

        // Layer 1: shapes.
        renderer.PushLayer(1);
        renderer.DrawRect(atom::render::Rect{24.0f, 24.0f, 240.0f, 120.0f}, atom::render::Color{220, 130, 60});
        renderer.DrawRectOutline(atom::render::Rect{24.0f, 24.0f, 240.0f, 120.0f}, atom::render::Color::White(), 3.0f);
        renderer.DrawCircle(160.0f, 210.0f, 46.0f, atom::render::Color{90, 190, 120});
        renderer.DrawLine(40.0f, 330.0f, 360.0f, 240.0f, atom::render::Color{120, 160, 255}, 4.0f);
        renderer.PopLayer();

        // Layer 2: atlas tiles (full, sub-rect, tinted).
        renderer.PushLayer(2);
        renderer.DrawTexture(*atlas, atom::render::Rect{420.0f, 40.0f, 128.0f, 128.0f});
        const atom::render::Rect bottomRight{64.0f, 64.0f, 64.0f, 64.0f};
        const atom::render::Rect topLeft{0.0f, 0.0f, 64.0f, 64.0f};
        renderer.DrawTexture(*atlas, atom::render::Rect{420.0f, 190.0f, 64.0f, 64.0f}, atom::render::Color::White(),
                             &bottomRight);
        renderer.DrawTexture(*atlas, atom::render::Rect{500.0f, 190.0f, 128.0f, 64.0f},
                             atom::render::Color{255, 220, 180}, &topLeft);
        // Decoded PNG (M3) drawn next to the synthetic atlas.
        renderer.DrawTexture(*photo, atom::render::Rect{700.0f, 40.0f, 200.0f, 200.0f});
        renderer.DrawTexture(*photo, atom::render::Rect{920.0f, 40.0f, 100.0f, 100.0f},
                             atom::render::Color{255, 255, 255}, nullptr);
        renderer.PopLayer();

        // Layer 3: clipped group (shapes crossing the clip window edges).
        renderer.PushLayer(3);
        renderer.PushClip(atom::render::Rect{40.0f, 380.0f, 300.0f, 120.0f});
        renderer.DrawRect(atom::render::Rect{20.0f, 370.0f, 360.0f, 60.0f}, atom::render::Color{60, 130, 180, 200});
        renderer.DrawCircle(200.0f, 430.0f, 90.0f, atom::render::Color{220, 90, 160, 180});
        renderer.DrawRectOutline(atom::render::Rect{100.0f, 340.0f, 260.0f, 90.0f}, atom::render::Color{0, 220, 220},
                                 6.0f);
        renderer.PopClip();
        renderer.DrawRect(atom::render::Rect{380.0f, 420.0f, 160.0f, 60.0f},
                          atom::render::Color{240, 210, 60, 220}); // outside clip, always visible
        renderer.PopLayer();

        // A thick animated frame drawn in front of everything.
        renderer.PushLayer(9);
        const float pulse = (std::sin(time * 2.0f) + 1.0f) * 0.5f;
        renderer.DrawRectOutline(atom::render::Rect{8.0f, 8.0f, w - 16.0f, h - 16.0f},
                                 atom::render::Color{static_cast<uint8_t>(120 + pulse * 100.0f), 200, 120}, 4.0f);
        renderer.PopLayer();

        if (!renderer.EndFrame())
            break;
        device.EndFrame();
        backend.Window().WaitForNextFrame();
    }

    renderer.Shutdown();
    backend.Shutdown();
    return 0;
}
