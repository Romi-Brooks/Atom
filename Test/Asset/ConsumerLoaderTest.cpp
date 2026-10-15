/**
 * @file           : ConsumerLoaderTests.cpp
 * @brief          : ScriptSourceLoader and DecodedImageLoader smoke tests.
**/

#include <Asset/ResourceManager.hpp>
#include <Asset/ScriptSourceLoader.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Media/Audio/Resources/AudioClipLoader.hpp>
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

// Declines every stream, to drive AudioClipLoader's "no candidate could open
// this" path without any real decoder.
class DecliningDecoder final : public atom::audio::IAudioDecoder {
    public:
        [[nodiscard]] auto OpenFromMemory(const void*, std::size_t) -> atom::audio::DecoderOpenStatus override {
            return atom::audio::DecoderOpenStatus::UnsupportedFormat;
        }
        [[nodiscard]] auto OpenStream(atom::fs::IFile&) -> atom::audio::DecoderOpenStatus override {
            return atom::audio::DecoderOpenStatus::UnsupportedFormat;
        }
        auto Close() -> void override {}
        auto DecodeChunk(uint8_t*, uint32_t) -> uint32_t override {
            return 0;
        }
        auto Rewind() -> bool override {
            return false;
        }
        [[nodiscard]] auto GetInfo() const -> const atom::audio::DecoderInfo& override {
            return info_;
        }
        [[nodiscard]] auto IsOpen() const -> bool override {
            return false;
        }

    private:
        atom::audio::DecoderInfo info_{44100, 1, 16, 1, false};
};

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

// Minimal valid 1x1 RGB PNG.
constexpr std::array<uint8_t, 78> tiny_png{
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8, 0xFF, 0xFF, 0x3F, 0x00, 0x05, 0xFE, 0x02, 0xFE, 0xA7,
    0x35, 0x81, 0x84, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

auto TinyPngBytes() -> std::vector<std::byte> {
    std::vector<std::byte> bytes(tiny_png.size());
    for (std::size_t index = 0; index < tiny_png.size(); ++index)
        bytes[index] = static_cast<std::byte>(tiny_png[index]);
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
    if (!manager.RegisterLoader(std::make_shared<atom::ScriptSourceLoader>()))
        return Fail("Could not register script loader");
    if (!manager.RegisterLoader(std::make_shared<atom::image::DecodedImageLoader>())) {
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

    // --- AudioClipLoader reports why a load failed (CORE-004) --------------
    // The loader used to return std::optional, so "the file is missing" and
    // "no decoder would open it" were the same nothing. They are now different
    // codes, and the error names the resource.
    {
        atom::audio::AudioDecoderRegistry decoders{};
        if (!decoders.Register("wav", [] { return std::make_unique<DecliningDecoder>(); }, "declining"))
            return Fail("Could not register the declining decoder");
        atom::AudioClipLoader loader{decoders};

        const auto missing = atom::fs::AssetPath::Parse("res://sounds/absent.wav");
        if (!missing.has_value())
            return Fail("Could not parse the missing-sound path");
        const auto missing_result = loader.Load(*memory, *missing);
        if (missing_result.has_value())
            return Fail("Loading a missing file succeeded");
        if (missing_result.error().Code() != atom::ErrorCode::FsNotFound)
            return Fail("A missing file did not report FsNotFound");
        if (missing_result.error().Resource() != "res://sounds/absent.wav")
            return Fail("The failure did not name the resource");

        // Present file, but every registered decoder declines it.
        const auto path = atom::fs::AssetPath::Parse("res://sounds/present.wav");
        if (!path.has_value())
            return Fail("Could not parse the present path");
        if (memory->WriteFile(*path, Bytes("not really a wav")) != atom::fs::Result::Success)
            return Fail("Could not seed the audio file");
        const auto declined = loader.Load(*memory, *path);
        if (declined.has_value())
            return Fail("A declined file was accepted");
        if (declined.error().Code() != atom::ErrorCode::AudioDecoderOpenFailed)
            return Fail("A declined file did not report AudioDecoderOpenFailed");
        if (declined.error().GetModule() != atom::Module::Audio)
            return Fail("The audio failure was attributed to the wrong module");

        // An unregistered extension is its own failure, not a decode failure.
        const auto unregistered = atom::fs::AssetPath::Parse("res://sounds/song.ogg");
        if (!unregistered.has_value())
            return Fail("Could not parse the unregistered path");
        if (memory->WriteFile(*unregistered, Bytes("ogg")) != atom::fs::Result::Success)
            return Fail("Could not seed the ogg file");
        const auto no_decoder = loader.Load(*memory, *unregistered);
        if (no_decoder.has_value())
            return Fail("A file with no registered decoder was accepted");
        if (no_decoder.error().Code() != atom::ErrorCode::AudioDecoderOpenFailed)
            return Fail("An unregistered extension did not report AudioDecoderOpenFailed");

        // The convenience overload reports a malformed URI as a path failure.
        const auto bad_uri = loader.Load("not-a-uri.mp3");
        if (bad_uri.has_value())
            return Fail("A malformed URI was accepted");
        if (bad_uri.error().Code() != atom::ErrorCode::FsMissingMount)
            return Fail("A malformed URI did not report the path rule it broke");
    }

    return 0;
}
