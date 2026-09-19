/**
 * @file           : ConsumerLoaderTests.cpp
 * @brief          : ScriptSourceLoader and DecodedImageLoader smoke tests.
**/

#include <Asset/ResourceManager.hpp>
#include <Asset/ScriptSourceLoader.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Media/Image/DecodedImageLoader.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

auto Fail(const std::string_view message) -> int {
    std::cerr << message << '\n';
    return 1;
}

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

// Minimal valid 1x1 RGB PNG.
constexpr std::array<uint8_t, 78> kTinyPng{
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8, 0xFF, 0xFF, 0x3F, 0x00, 0x05, 0xFE, 0x02, 0xFE, 0xA7,
    0x35, 0x81, 0x84, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

auto TinyPngBytes() -> std::vector<std::byte> {
    std::vector<std::byte> bytes(kTinyPng.size());
    for (std::size_t index = 0; index < kTinyPng.size(); ++index)
        bytes[index] = static_cast<std::byte>(kTinyPng[index]);
    return bytes;
}

} // namespace

auto main() -> int {
    std::unique_ptr<atom::fs::MemoryFileSystem> memory_owned{};
    if (atom::fs::MemoryFileSystem::Create("res", memory_owned) != atom::fs::Result::Success || !memory_owned)
        return Fail("Could not create memory filesystem");
    auto memory = std::shared_ptr<atom::fs::MemoryFileSystem>{std::move(memory_owned)};

    atom::fs::AssetPath script_path{};
    atom::fs::AssetPath image_path{};
    atom::fs::AssetPath bad_image_path{};
    if (!atom::fs::AssetPath::TryParse("res://scripts/main.lua", script_path) ||
        !atom::fs::AssetPath::TryParse("res://textures/pixel.png", image_path) ||
        !atom::fs::AssetPath::TryParse("res://textures/broken.png", bad_image_path)) {
        return Fail("Could not parse asset paths");
    }

    if (memory->WriteFile(script_path, Bytes("return 1 + 1")) != atom::fs::Result::Success)
        return Fail("Could not write script");
    if (memory->WriteFile(image_path, TinyPngBytes()) != atom::fs::Result::Success)
        return Fail("Could not write png");
    if (memory->WriteFile(bad_image_path, Bytes("not-a-png")) != atom::fs::Result::Success)
        return Fail("Could not write broken image");

    atom::asset::ResourceId script_id{};
    atom::asset::ResourceId image_id{};
    atom::asset::ResourceId bad_image_id{};
    if (!atom::asset::ResourceId::TryCreate(script_path, atom::asset::AssetKind::Script, "", script_id) ||
        !atom::asset::ResourceId::TryCreate(image_path, atom::asset::AssetKind::Texture, "", image_id) ||
        !atom::asset::ResourceId::TryCreate(bad_image_path, atom::asset::AssetKind::Texture, "", bad_image_id)) {
        return Fail("Could not create resource ids");
    }

    atom::asset::ResourceManager manager{*memory};
    if (manager.RegisterLoader(std::make_shared<atom::ScriptSourceLoader>()) != atom::asset::AssetResult::Success)
        return Fail("Could not register script loader");
    if (manager.RegisterLoader(std::make_shared<atom::image::DecodedImageLoader>()) !=
        atom::asset::AssetResult::Success) {
        return Fail("Could not register image loader");
    }

    auto script = manager.Acquire<std::string>(script_id);
    if (!script || *script != "return 1 + 1")
        return Fail("ScriptSourceLoader produced wrong source");
    auto script_again = manager.Acquire<std::string>(script_id);
    if (script.Get() != script_again.Get())
        return Fail("Script source was not shared");

    auto image = manager.Acquire<atom::image::DecodedImage>(image_id);
    if (!image || !image->IsValid() || image->width != 1 || image->height != 1 || image->rgba.size() != 4)
        return Fail("DecodedImageLoader did not produce a 1x1 RGBA image");

    if (manager.Acquire<atom::image::DecodedImage>(bad_image_id))
        return Fail("Broken image was accepted");

    return 0;
}
