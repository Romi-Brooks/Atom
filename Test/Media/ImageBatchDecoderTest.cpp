/**
 * @file           : ImageBatchDecoderTest.cpp
 * @brief          : Contract tests for ImageBatchDecoder::DecodeBatch — the
 *                   parallel batch decode entry point over the engine job
 *                   system. Mirrors AudioMetadataReaderTest: input-order
 *                   results and the sequential fallback when the job system is
 *                   not running.
 *
 * Real images are synthesized in-memory as minimal uncompressed 24-bit BMP
 * files, so the test is self-contained and decodes through stb_image exactly
 * as production code does.
**/

#include <Concurrency/JobSystem.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Media/Image/ImageBatchDecoder.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using atom::image::DecodedImage;
using atom::image::ImageBatchDecoder;
using atom::fs::AssetPath;

// --- little-endian BMP builder -------------------------------------------

auto AppendU8(std::vector<std::byte>& out, const std::uint8_t value) -> void {
    out.push_back(static_cast<std::byte>(value));
}

auto AppendU16(std::vector<std::byte>& out, const std::uint16_t value) -> void {
    out.push_back(static_cast<std::byte>(value & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFFu));
}

auto AppendU32(std::vector<std::byte>& out, const std::uint32_t value) -> void {
    out.push_back(static_cast<std::byte>(value & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 24) & 0xFFu));
}

// Synthesizes a width x height uncompressed 24-bit BMP (solid white). Only the
// header and dimensions matter; stb_image derives width/height from the info
// header, not from the pixel values.
auto MakeBmp(const std::uint32_t width, const std::uint32_t height) -> std::vector<std::byte> {
    const std::uint32_t row_size = (width * 3u + 3u) & ~3u; // 24-bit, padded to 4 bytes
    const std::uint32_t pixel_data_size = row_size * height;
    constexpr std::uint32_t kDataOffset = 14 + 40; // file header + info header
    const std::uint32_t file_size = kDataOffset + pixel_data_size;

    std::vector<std::byte> bmp{};
    bmp.reserve(file_size);
    // BITMAPFILEHEADER
    AppendU8(bmp, 'B');
    AppendU8(bmp, 'M');
    AppendU32(bmp, file_size);
    AppendU32(bmp, 0);           // reserved
    AppendU32(bmp, kDataOffset); // pixel data offset
    // BITMAPINFOHEADER (40 bytes)
    AppendU32(bmp, 40);           // header size
    AppendU32(bmp, width);        // width (positive int32)
    AppendU32(bmp, height);       // height (positive int32 = bottom-up)
    AppendU16(bmp, 1);            // planes
    AppendU16(bmp, 24);           // bits per pixel
    AppendU32(bmp, 0);            // compression: BI_RGB
    AppendU32(bmp, pixel_data_size);
    AppendU32(bmp, 2835);         // x pixels per meter (~72 dpi)
    AppendU32(bmp, 2835);         // y pixels per meter
    AppendU32(bmp, 0);            // colors used
    AppendU32(bmp, 0);            // colors important
    // Pixel data: BGR rows, bottom-up, padded to 4-byte boundaries.
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            AppendU8(bmp, 0xFF); // blue
            AppendU8(bmp, 0xFF); // green
            AppendU8(bmp, 0xFF); // red
        }
        for (std::uint32_t p = width * 3u; p < row_size; ++p)
            AppendU8(bmp, 0); // row padding
    }
    return bmp;
}

// The batch under test is always [bmp0, missing, bmp1, bmp2]: a missing file
// in the middle proves both input-order preservation and that a failure slot
// does not shift its neighbours.
auto VerifyBatch(const std::vector<std::optional<DecodedImage>>& results) -> int {
    ATOM_CHECK(results.size() == 4);

    ATOM_CHECK(results[0].has_value());
    ATOM_CHECK(results[0]->width == 2);
    ATOM_CHECK(results[0]->height == 2);
    ATOM_CHECK(results[0]->IsValid());

    ATOM_CHECK(!results[1].has_value()); // missing → nullopt, order intact

    ATOM_CHECK(results[2].has_value());
    ATOM_CHECK(results[2]->width == 3);
    ATOM_CHECK(results[2]->height == 3);
    ATOM_CHECK(results[2]->IsValid());

    ATOM_CHECK(results[3].has_value());
    ATOM_CHECK(results[3]->width == 4);
    ATOM_CHECK(results[3]->height == 4);
    ATOM_CHECK(results[3]->IsValid());

    return 0;
}

} // namespace

auto main() -> int {
    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return atom::test::Fail("Could not create memory filesystem");

    AssetPath bmp0{};
    AssetPath bmp1{};
    AssetPath bmp2{};
    AssetPath missing{};
    if (!AssetPath::TryParse("res://img0.bmp", bmp0) || !AssetPath::TryParse("res://img1.bmp", bmp1) ||
        !AssetPath::TryParse("res://img2.bmp", bmp2) || !AssetPath::TryParse("res://missing.bmp", missing))
        return atom::test::Fail("Could not parse asset paths");
    if (memory->WriteFile(bmp0, MakeBmp(2, 2)) != atom::fs::Result::Success ||
        memory->WriteFile(bmp1, MakeBmp(3, 3)) != atom::fs::Result::Success ||
        memory->WriteFile(bmp2, MakeBmp(4, 4)) != atom::fs::Result::Success)
        return atom::test::Fail("Could not write the synthesized BMP files");

    const std::vector<AssetPath> batch{bmp0, missing, bmp1, bmp2};

    // --- Phase 1: job system not running → sequential fallback ------------
    {
        auto& jobs = atom::job::JobSystem::GetInstance();
        if (jobs.IsRunning())
            jobs.Shutdown();
        ATOM_CHECK(!jobs.IsRunning());

        const auto results = ImageBatchDecoder::DecodeBatch(*memory, batch);
        if (VerifyBatch(results) != 0)
            return 1;
    }

    // --- Phase 2: job system running → parallel decodes -------------------
    {
        auto& jobs = atom::job::JobSystem::GetInstance();
        jobs.Configure(4);
        ATOM_CHECK(jobs.Start());
        ATOM_CHECK(jobs.IsRunning());

        const auto results = ImageBatchDecoder::DecodeBatch(*memory, batch);
        if (VerifyBatch(results) != 0)
            return 1;

        jobs.Shutdown();
    }

    return 0;
}
