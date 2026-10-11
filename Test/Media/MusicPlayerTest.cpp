/**
 * @file           : MusicPlayerTest.cpp
 * @brief          : MusicPlayer track lifecycle — every Load() must have a
 *                   matching release path, so walking a large library cannot pin
 *                   every decoder, streaming buffer and open file for the
 *                   player's whole lifetime.
**/

#include <Backend/Contracts/Audio/IAudioBackend.hpp>
#include <Backend/Contracts/Audio/IAudioSource.hpp>
#include <Backend/Extension/AudioDecoderRegistry.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Media/Audio/Mixing/AudioMixer.hpp>
#include <Media/Audio/Playback/MusicPlayer.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace {

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

// A decoder that "opens" any stream: the lifecycle under test belongs to
// MusicPlayer, not to any real format.
class StubDecoder final : public atom::audio::IAudioDecoder {
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
        auto DecodeChunk(uint8_t*, uint32_t) -> uint32_t override {
            return 0;
        }
        auto Rewind() -> bool override {
            return true;
        }
        [[nodiscard]] auto GetInfo() const -> const atom::audio::DecoderInfo& override {
            return info_;
        }
        [[nodiscard]] auto IsOpen() const -> bool override {
            return opened_;
        }

    private:
        atom::audio::DecoderInfo info_{44100, 2, 16, 0, false};
        bool opened_ = false;
};

// Counts live sources, so the test proves Unload() destroyed the source (and
// with it the decoder and the streaming buffer) instead of only forgetting its
// id.
class StubSource final : public atom::audio::IAudioSource {
    public:
        explicit StubSource(int& live_count) : live_count_(live_count) {
            ++live_count_;
        }
        ~StubSource() override {
            --live_count_;
        }

        auto Play() -> void override {
            state_ = atom::audio::AudioSourceState::Playing;
        }
        auto Stop() -> void override {
            state_ = atom::audio::AudioSourceState::Stopped;
        }
        auto Pause() -> void override {
            state_ = atom::audio::AudioSourceState::Paused;
        }
        [[nodiscard]] auto GetState() const -> atom::audio::AudioSourceState override {
            return state_;
        }
        auto SetVolume(float) -> void override {}
        [[nodiscard]] auto GetVolume() const -> float override {
            return 100.0f;
        }
        auto SetLooping(bool loop) -> void override {
            loop_ = loop;
        }
        [[nodiscard]] auto IsLooping() const -> bool override {
            return loop_;
        }
        auto SetPlayingOffset(float) -> bool override {
            return true;
        }
        [[nodiscard]] auto GetPlayingOffset() const -> float override {
            return 0.0f;
        }
        [[nodiscard]] auto IsSeekable() const -> bool override {
            return true;
        }

    private:
        int& live_count_;
        atom::audio::AudioSourceState state_ = atom::audio::AudioSourceState::Stopped;
        bool loop_ = false;
};

class StubBackend final : public atom::audio::IAudioBackend {
    public:
        [[nodiscard]] auto CreateMusicSource(std::vector<uint8_t>, const atom::audio::AudioSpec&)
            -> std::unique_ptr<atom::audio::IAudioSource> override {
            return nullptr;
        }
        [[nodiscard]] auto CreateSFXSource(const std::vector<uint8_t>&, const atom::audio::AudioSpec&)
            -> std::unique_ptr<atom::audio::IAudioSource> override {
            return nullptr;
        }
        [[nodiscard]] auto CreateStreamingMusicSource(std::unique_ptr<atom::audio::IAudioDecoder> decoder,
                                                      const atom::audio::AudioSpec&)
            -> std::unique_ptr<atom::audio::IAudioSource> override {
            if (!decoder || !decoder->IsOpen())
                return nullptr;
            ++created_sources_;
            return std::make_unique<StubSource>(live_sources_);
        }

        [[nodiscard]] auto LiveSources() const -> int {
            return live_sources_;
        }
        [[nodiscard]] auto CreatedSources() const -> int {
            return created_sources_;
        }

    private:
        int live_sources_ = 0;
        int created_sources_ = 0;
};

} // namespace

auto main() -> int {
    atom::AudioMixer mixer{};
    StubBackend backend{};
    atom::audio::AudioDecoderRegistry decoders{};
    if (!decoders.Register("wav", [] { return std::make_unique<StubDecoder>(); }, "stub-wav"))
        return atom::test::Fail("Could not register the stub decoder");
    atom::MusicPlayer player{backend, decoders, mixer};

    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return atom::test::Fail("Could not create memory filesystem");
    atom::fs::AssetPath track_path{};
    atom::fs::AssetPath other_path{};
    if (!atom::fs::AssetPath::TryParse("res://track.wav", track_path) ||
        !atom::fs::AssetPath::TryParse("res://other.wav", other_path))
        return atom::test::Fail("Could not parse asset paths");
    if (memory->WriteFile(track_path, Bytes("RIFF-stub")) != atom::fs::Result::Success ||
        memory->WriteFile(other_path, Bytes("RIFF-stub")) != atom::fs::Result::Success)
        return atom::test::Fail("Could not write the stub track files");

    // --- Unknown ids are rejected and never reported as loaded. ---
    ATOM_CHECK(!player.Unload("missing"));
    ATOM_CHECK(!player.IsLoaded("missing"));

    // --- Load / Unload round trip ---
    ATOM_CHECK(player.Load("track", *memory, track_path));
    ATOM_CHECK(player.IsLoaded("track"));
    // Loading an id that is already loaded is an idempotent success, not a
    // second source.
    ATOM_CHECK(player.Load("track", *memory, track_path));
    ATOM_CHECK(backend.LiveSources() == 1);
    ATOM_CHECK(backend.CreatedSources() == 1);

    ATOM_CHECK(player.Load("other", *memory, other_path));
    ATOM_CHECK(backend.LiveSources() == 2);

    // Unloading one track releases exactly that source.
    ATOM_CHECK(player.Unload("track"));
    ATOM_CHECK(!player.IsLoaded("track"));
    ATOM_CHECK(player.IsLoaded("other"));
    ATOM_CHECK(backend.LiveSources() == 1);

    // Unloading again, or an id that was never loaded, is a reported no-op.
    ATOM_CHECK(!player.Unload("track"));

    // Unloading the playing track clears the now-playing id.
    player.Play("other");
    ATOM_CHECK(player.GetNowPlaying() == "other");
    ATOM_CHECK(player.Unload("other"));
    ATOM_CHECK(player.GetNowPlaying().empty());
    ATOM_CHECK(backend.LiveSources() == 0);

    // --- Reset() releases everything that is still loaded ---
    ATOM_CHECK(player.Load("a", *memory, track_path));
    ATOM_CHECK(player.Load("b", *memory, other_path));
    ATOM_CHECK(backend.LiveSources() == 2);
    player.Reset();
    ATOM_CHECK(backend.LiveSources() == 0);
    ATOM_CHECK(!player.IsLoaded("a"));
    ATOM_CHECK(!player.IsLoaded("b"));
    return 0;
}
