/**
 * @file           : AudioMetadataReaderTest.cpp
 * @brief          : Contract tests for AudioMetadataReader::ReadBatch — the
 *                   parallel batch entry point over the engine job system.
 *
 * Two behaviours matter here:
 *   1. Results come back in input order, even when a mid-batch file is missing
 *      (that slot must be nullopt without shifting its neighbours).
 *   2. When the job system is not running, ReadBatch silently falls back to
 *      sequential reads, so callers never have to reason about scheduler state.
 *
 * Real audio is synthesized in-memory as minimal PCM WAV files so the test is
 * fully self-contained: TagLib parses them through the VFS IOStream adapter,
 * exactly as production code does.
**/

#include <Concurrency/JobSystem.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Media/Audio/Metadata/AudioMetadataReader.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using atom::audio::AudioMetadata;
using atom::audio::AudioMetadataReader;
using atom::fs::AssetPath;

// --- little-endian WAV builder -------------------------------------------

auto AppendU16(std::vector<std::byte>& out, const uint16_t value) -> void {
    out.push_back(static_cast<std::byte>(value & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFFu));
}

auto AppendU32(std::vector<std::byte>& out, const uint32_t value) -> void {
    out.push_back(static_cast<std::byte>(value & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 24) & 0xFFu));
}

auto AppendFourCc(std::vector<std::byte>& out, const std::string_view four_cc) -> void {
    for (const char c : four_cc)
        out.push_back(static_cast<std::byte>(c));
}

// Synthesizes a one-second mono 16-bit PCM WAV at `sample_rate` Hz. The data
// chunk is pure silence; only the header matters, since TagLib derives the
// properties from the "fmt " and "data" chunks, not from the samples.
auto MakeWav(const uint32_t sample_rate) -> std::vector<std::byte> {
    constexpr uint16_t kChannels = 1;
    constexpr uint16_t kBitsPerSample = 16;
    constexpr uint16_t kBlockAlign = kChannels * (kBitsPerSample / 8);
    constexpr uint32_t kFmtSize = 16;
    const uint32_t byte_rate = sample_rate * kBlockAlign;
    const uint32_t data_size = sample_rate * kBlockAlign; // one second of frames
    const uint32_t riff_size = 4 + (8 + kFmtSize) + (8 + data_size);

    std::vector<std::byte> wav{};
    wav.reserve(8 + riff_size);
    AppendFourCc(wav, "RIFF");
    AppendU32(wav, riff_size);
    AppendFourCc(wav, "WAVE");
    AppendFourCc(wav, "fmt ");
    AppendU32(wav, kFmtSize);
    AppendU16(wav, 1); // PCM
    AppendU16(wav, kChannels);
    AppendU32(wav, sample_rate);
    AppendU32(wav, byte_rate);
    AppendU16(wav, kBlockAlign);
    AppendU16(wav, kBitsPerSample);
    AppendFourCc(wav, "data");
    AppendU32(wav, data_size);
    wav.resize(wav.size() + data_size, std::byte{0}); // silence
    return wav;
}

// --- batch contract verification -----------------------------------------

// The batch under test is always [track0, missing, track1, track2]: a missing
// file in the middle proves both input-order preservation and that a failure
// slot does not shift its neighbours.
auto VerifyBatch(const std::vector<std::optional<AudioMetadata>>& results, const uint32_t rate0,
                 const uint32_t rate1, const uint32_t rate2) -> int {
    ATOM_CHECK(results.size() == 4);

    // track0 → rate0
    ATOM_CHECK(results[0].has_value());
    ATOM_CHECK(results[0]->sampleRate == rate0);
    ATOM_CHECK(results[0]->channels == 1);
    ATOM_CHECK(results[0]->durationSeconds == 1);

    // missing → nullopt, without disturbing order
    ATOM_CHECK(!results[1].has_value());

    // track1 → rate1
    ATOM_CHECK(results[2].has_value());
    ATOM_CHECK(results[2]->sampleRate == rate1);
    ATOM_CHECK(results[2]->channels == 1);

    // track2 → rate2
    ATOM_CHECK(results[3].has_value());
    ATOM_CHECK(results[3]->sampleRate == rate2);
    ATOM_CHECK(results[3]->channels == 1);

    return 0;
}

} // namespace

auto main() -> int {
    constexpr uint32_t kRate0 = 8000;
    constexpr uint32_t kRate1 = 16000;
    constexpr uint32_t kRate2 = 44100;

    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return atom::test::Fail("Could not create memory filesystem");

    // Write three distinct files plus a deliberately missing one.
    AssetPath track0{};
    AssetPath track1{};
    AssetPath track2{};
    AssetPath missing{};
    if (!AssetPath::TryParse("res://track0.wav", track0) || !AssetPath::TryParse("res://track1.wav", track1) ||
        !AssetPath::TryParse("res://track2.wav", track2) || !AssetPath::TryParse("res://missing.wav", missing))
        return atom::test::Fail("Could not parse asset paths");
    if (memory->WriteFile(track0, MakeWav(kRate0)) != atom::fs::Result::Success ||
        memory->WriteFile(track1, MakeWav(kRate1)) != atom::fs::Result::Success ||
        memory->WriteFile(track2, MakeWav(kRate2)) != atom::fs::Result::Success)
        return atom::test::Fail("Could not write the synthesized WAV files");

    const std::vector<AssetPath> batch{track0, missing, track1, track2};

    // --- Phase 1: job system not running → sequential fallback ------------
    {
        auto& jobs = atom::job::JobSystem::GetInstance();
        if (jobs.IsRunning())
            jobs.Shutdown();
        ATOM_CHECK(!jobs.IsRunning());

        const auto results = AudioMetadataReader::ReadBatch(*memory, batch);
        if (VerifyBatch(results, kRate0, kRate1, kRate2) != 0)
            return 1;
    }

    // --- Phase 2: job system running → parallel reads ---------------------
    {
        auto& jobs = atom::job::JobSystem::GetInstance();
        jobs.Configure(4);
        ATOM_CHECK(jobs.Start());
        ATOM_CHECK(jobs.IsRunning());

        const auto results = AudioMetadataReader::ReadBatch(*memory, batch);
        if (VerifyBatch(results, kRate0, kRate1, kRate2) != 0)
            return 1;

        jobs.Shutdown();
    }

    return 0;
}
