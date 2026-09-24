/**
 * @file           : StageDAcceptanceTest.cpp
 * @brief          : ARCH-107 Stage D — unified URI loading across directory and
 *                   package mounts, with resource sharing.
 *
 * Verifies the acceptance criterion in Docs/Filesystem-Design-CN.md: the same
 * URI opens from a directory mount and an APKG mount, and Texture / Audio /
 * Script all load and share a single instance through ResourceManager.
**/

#include <Asset/ResourceManager.hpp>
#include <Asset/ScriptSourceLoader.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Filesystem/PackageFileSystem.hpp>
#include <Filesystem/Vfs.hpp>
#include <Media/Audio/Resources/AudioClipResourceLoader.hpp>
#include <Media/Image/DecodedImageLoader.hpp>
#include <Backend/Contracts/Audio/IAudioDecoder.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Test/Support/TestHelpers.hpp>
#include <Utilities/Utf8/Utf8.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace native_fs = std::filesystem;

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

// Minimal valid 1x1 RGB PNG (same as ConsumerLoaderTest).
constexpr std::array<uint8_t, 78> kTinyPng{
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8, 0xFF, 0xFF, 0x3F, 0x00, 0x05, 0xFE, 0x02, 0xFE, 0xA7,
    0x35, 0x81, 0x84, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0x8D, 0x42, 0x60, 0x82,
};

auto TinyPngBytes() -> std::vector<std::byte> {
    std::vector<std::byte> bytes(kTinyPng.size());
    for (std::size_t index = 0; index < kTinyPng.size(); ++index)
        bytes[index] = static_cast<std::byte>(kTinyPng[index]);
    return bytes;
}

// A self-contained decoder that "succeeds" on any .wav via OpenStream, so the
// Stage-D test exercises the Audio loader's VFS + sharing path without pulling
// in SDL3 or a real backend. Correct per-format decoding is covered elsewhere.
class StubWavDecoder final : public atom::audio::IAudioDecoder {
    public:
        [[nodiscard]] auto OpenFromMemory(const void*, std::size_t) -> atom::audio::DecoderOpenStatus override {
            return atom::audio::DecoderOpenStatus::UnsupportedFormat;
        }
        [[nodiscard]] auto OpenStream(atom::fs::IFile&) -> atom::audio::DecoderOpenStatus override {
            opened_ = true;
            return atom::audio::DecoderOpenStatus::Opened;
        }
        auto Close() -> void override {
            opened_ = false;
        }
        auto DecodeChunk(uint8_t* output, uint32_t max_bytes) -> uint32_t override {
            if (emitted_)
                return 0;
            emitted_ = true;
            // Emit one mono 16-bit frame (2 bytes).
            if (max_bytes < 2)
                return 0;
            output[0] = 0x00;
            output[1] = 0x00;
            return 2;
        }
        auto Rewind() -> bool override {
            emitted_ = false;
            return true;
        }
        [[nodiscard]] auto GetInfo() const -> const atom::audio::DecoderInfo& override {
            return info_;
        }
        [[nodiscard]] auto IsOpen() const -> bool override {
            return opened_;
        }

    private:
        atom::audio::DecoderInfo info_{44100, 1, 16, 1, false};
        bool opened_ = false;
        bool emitted_ = false;
};

class TemporaryDirectory final {
    public:
        [[nodiscard]] static auto Create(TemporaryDirectory& output) -> bool {
            std::error_code error;
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            const auto root =
                native_fs::temp_directory_path(error) / ("atom_stage_d_test_" + std::to_string(nonce));
            if (error || native_fs::exists(root, error))
                return false;
            if (!native_fs::create_directories(root, error) || error)
                return false;
            output.root_ = root;
            return true;
        }
        ~TemporaryDirectory() {
            if (!root_.empty()) {
                std::error_code error;
                native_fs::remove_all(root_, error);
            }
        }
        TemporaryDirectory() = default;
        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
        [[nodiscard]] auto Root() const -> const native_fs::path& {
            return root_;
        }

    private:
        native_fs::path root_{};
};

// Minimal APKG v1 writer (mirrors Utilities/Packager, binary-safe).
auto WriteApkg(const native_fs::path& package,
               const std::vector<std::pair<std::string, std::vector<std::byte>>>& files) -> bool {
    std::ofstream output{package, std::ios::binary | std::ios::trunc};
    if (!output.is_open())
        return false;

    output.write("APKG", 4);
    const uint16_t version = 1;
    output.write(reinterpret_cast<const char*>(&version), sizeof(version));
    const uint32_t count = static_cast<uint32_t>(files.size());
    output.write(reinterpret_cast<const char*>(&count), sizeof(count));

    struct Row {
            std::string name{};
            uint64_t offset = 0;
            uint64_t size = 0;
    };
    std::vector<Row> rows{};
    rows.reserve(files.size());
    for (const auto& [name, data] : files) {
        Row row{name, static_cast<uint64_t>(output.tellp()), data.size()};
        output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        rows.push_back(std::move(row));
    }

    const auto table_offset = static_cast<uint64_t>(output.tellp());
    for (const auto& row : rows) {
        const auto name_length = static_cast<uint16_t>(row.name.size());
        output.write(reinterpret_cast<const char*>(&name_length), sizeof(name_length));
        output.write(row.name.data(), name_length);
        output.write(reinterpret_cast<const char*>(&row.offset), sizeof(row.offset));
        output.write(reinterpret_cast<const char*>(&row.size), sizeof(row.size));
        const uint8_t type_length = 0;
        output.write(reinterpret_cast<const char*>(&type_length), sizeof(type_length));
    }
    output.write(reinterpret_cast<const char*>(&table_offset), sizeof(table_offset));
    return static_cast<bool>(output);
}

} // namespace

auto main() -> int {
    using namespace atom::asset;

    // --- Audio decoder registry for the AudioClipResourceLoader (stub wav). ---
    atom::audio::AudioDecoderRegistry decoders{};
    ATOM_CHECK(decoders.Register("wav", [] { return std::make_unique<StubWavDecoder>(); }, "stub-wav"));

    // --- A "development directory" backend (in-memory) and an APKG backend. ---
    std::unique_ptr<atom::fs::MemoryFileSystem> dir_owned{};
    ATOM_CHECK(atom::fs::MemoryFileSystem::Create("res", dir_owned) == atom::fs::Result::Success && dir_owned);
    auto directory = std::shared_ptr<atom::fs::MemoryFileSystem>{std::move(dir_owned)};

    TemporaryDirectory temporary{};
    ATOM_CHECK(TemporaryDirectory::Create(temporary));
    const native_fs::path package_path = temporary.Root() / "game.apkg";
    ATOM_CHECK(WriteApkg(package_path,
                         {
                             {"textures/pixel.png", TinyPngBytes()},
                             {"sounds/shot.wav", Bytes("RIFF-stub")},
                             {"scripts/main.lua", Bytes("return 1 + 1")},
                         }));

    std::unique_ptr<atom::fs::PackageFileSystem> pkg_owned{};
    ATOM_CHECK(atom::fs::PackageFileSystem::Create("res", atom::PathToUtf8(package_path), pkg_owned) ==
                   atom::fs::Result::Success &&
               pkg_owned);
    auto packaged = std::shared_ptr<atom::fs::PackageFileSystem>{std::move(pkg_owned)};

    // --- VFS: directory at priority 200 (overrides), package at 100 (base). ---
    atom::fs::Vfs vfs{};
    ATOM_CHECK(vfs.Mount("res", 200, directory) == atom::fs::Result::Success);
    ATOM_CHECK(vfs.Mount("res", 100, packaged) == atom::fs::Result::Success);

    // --- Asset identities for the three resource kinds. ---
    atom::fs::AssetPath texture_path{};
    atom::fs::AssetPath audio_path{};
    atom::fs::AssetPath script_path{};
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://textures/pixel.png", texture_path));
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://sounds/shot.wav", audio_path));
    ATOM_CHECK(atom::fs::AssetPath::TryParse("res://scripts/main.lua", script_path));

    ResourceId texture_id{};
    ResourceId audio_id{};
    ResourceId script_id{};
    ATOM_CHECK(ResourceId::TryCreate(texture_path, AssetKind::Texture, "", texture_id));
    ATOM_CHECK(ResourceId::TryCreate(audio_path, AssetKind::Audio, "", audio_id));
    ATOM_CHECK(ResourceId::TryCreate(script_path, AssetKind::Script, "", script_id));

    // --- ResourceManager with all three loaders (unified URI machinery). ---
    ResourceManager manager{vfs};
    ATOM_CHECK(manager.RegisterLoader(std::make_shared<atom::image::DecodedImageLoader>()) == AssetResult::Success);
    ATOM_CHECK(manager.RegisterLoader(std::make_shared<atom::AudioClipResourceLoader>(decoders)) ==
               AssetResult::Success);
    ATOM_CHECK(manager.RegisterLoader(std::make_shared<atom::ScriptSourceLoader>()) == AssetResult::Success);

    // --- Texture: loads from the package, shares one instance. ---
    auto texture = manager.Acquire<atom::image::DecodedImage>(texture_id);
    ATOM_CHECK(texture && texture->IsValid() && texture->width == 1 && texture->height == 1);
    auto texture_again = manager.Acquire<atom::image::DecodedImage>(texture_id);
    ATOM_CHECK(texture_again.Get() == texture.Get());

    // --- Script: loads and shares one instance. ---
    auto script = manager.Acquire<std::string>(script_id);
    ATOM_CHECK(script && *script == "return 1 + 1");
    auto script_again = manager.Acquire<std::string>(script_id);
    ATOM_CHECK(script_again.Get() == script.Get());

    // --- Audio: loads through the VFS and shares one instance. ---
    auto audio = manager.Acquire<atom::audio::DecodedAudio>(audio_id);
    ATOM_CHECK(audio && !audio->pcm.empty() && audio->spec.sample_rate == 44100 && audio->spec.channels == 1);
    auto audio_again = manager.Acquire<atom::audio::DecodedAudio>(audio_id);
    ATOM_CHECK(audio_again.Get() == audio.Get());

    // --- Cross-mount equivalence: a directory override of the script is picked
    // up when mounted at higher priority, yet still resolves the same URI. ---
    ATOM_CHECK(directory->WriteFile(script_path, Bytes("return 2 + 2")) == atom::fs::Result::Success);
    // The manager already cached the script; evict to force a reload from the
    // now-overridden directory source.
    ATOM_CHECK(manager.Evict(script_id));
    auto script_overridden = manager.Acquire<std::string>(script_id);
    ATOM_CHECK(script_overridden && *script_overridden == "return 2 + 2");

    // --- Non-shared kinds must not cross-type leak: the texture path requested
    // as a script must yield nothing. ---
    ATOM_CHECK(!manager.Acquire<std::string>(texture_id));

    return 0;
}
