/**
 * @file           : ImageDecoderTest.cpp
 * @brief          : Image decoding contract, including the ARCH-006 guard that
 *                   rejects an oversized header before stb_image allocates.
 */

#include <Log/LogSystem.hpp>
#include <Media/Image/ImageDecoder.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace {

// Minimal valid 1x1 RGB PNG (same fixture as ConsumerLoaderTest / Stage D).
constexpr std::array<uint8_t, 78> kTinyPng{
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8, 0xFF, 0xFF, 0x3F, 0x00, 0x05, 0xFE, 0x02, 0xFE, 0xA7,
    0x35, 0x81, 0x84, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0x8D, 0x42, 0x60, 0x82,
};

// 54-byte BITMAPFILEHEADER + BITMAPINFOHEADER describing 20000 x 20000, 24 bpp.
//
// stb's header probe accepts this (stb only bounds BMP by the 32-bit product of
// the dimensions, and 20000 * 20000 * 3 still fits in int32), so without the
// ARCH-006 decoded-surface budget stbi_load_from_memory would allocate ~1.2 GB
// before the image could be rejected.
constexpr std::array<uint8_t, 54> kOversizedBmpHeader{
    0x42, 0x4D,                   // "BM"
    0x36, 0x00, 0x00, 0x00,       // file size (54, ignored by the probe)
    0x00, 0x00, 0x00, 0x00,       // reserved
    0x36, 0x00, 0x00, 0x00,       // pixel data offset
    0x28, 0x00, 0x00, 0x00,       // info header size (40)
    0x20, 0x4E, 0x00, 0x00,       // width  = 20000
    0x20, 0x4E, 0x00, 0x00,       // height = 20000
    0x01, 0x00,                   // planes
    0x18, 0x00,                   // bits per pixel (24)
    0x00, 0x00, 0x00, 0x00,       // compression (none)
    0x00, 0x00, 0x00, 0x00,       // image size
    0x00, 0x00, 0x00, 0x00,       // x pixels per meter
    0x00, 0x00, 0x00, 0x00,       // y pixels per meter
    0x00, 0x00, 0x00, 0x00,       // palette colours used
    0x00, 0x00, 0x00, 0x00,       // important colours
};

// Same header layout, but 1 x 20000: a thin strip whose decoded surface is only
// ~60 KB. The budget bounds the total surface, not each axis, so this must pass
// the pre-decode guard (whether the truncated pixel data then decodes is a
// separate matter).
constexpr std::array<uint8_t, 54> kThinBmpHeader{
    0x42, 0x4D, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
    0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x20, 0x4E, 0x00, 0x00, 0x01, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

template <typename TContainer>
auto SpanOf(const TContainer& data) -> std::span<const std::byte> {
    return {reinterpret_cast<const std::byte*>(data.data()), data.size()};
}

} // namespace

auto main() -> int {
    // Capture the decoder's diagnostics without writing to the console.
    atom::Log::SetConsoleSinkEnabled(false);
    std::vector<atom::LogRecord> records{};
    auto connection = atom::Log::Subscribe([&records](const atom::LogRecord& record) { records.push_back(record); });
    const auto saw_message = [&records](const std::string& needle) {
        for (const auto& record : records) {
            if (record.message.find(needle) != std::string::npos)
                return true;
        }
        return false;
    };

    // A valid image decodes to a 1x1 RGBA surface.
    const auto tiny = atom::image::DecodeImageMemory(SpanOf(kTinyPng));
    ATOM_CHECK(tiny.IsValid());
    ATOM_CHECK(tiny.width == 1 && tiny.height == 1);
    ATOM_CHECK(tiny.rgba.size() == 4);

    // The flipped variant stays valid and same-sized.
    const auto flipped = atom::image::DecodeImageMemory(SpanOf(kTinyPng), true);
    ATOM_CHECK(flipped.IsValid());
    ATOM_CHECK(flipped.width == 1 && flipped.height == 1);

    // ARCH-006: an oversized header is refused *before* the decode, so no
    // width*height*bpp allocation happens.
    records.clear();
    const auto oversized = atom::image::DecodeImageMemory(SpanOf(kOversizedBmpHeader));
    ATOM_CHECK(!oversized.IsValid());
    ATOM_CHECK(oversized.width == 0 && oversized.height == 0);
    ATOM_CHECK(saw_message("rejected before decode"));
    ATOM_CHECK(saw_message("exceeds the decoded-image budget"));

    // The budget is on the decoded surface as a whole, so a legitimate thin
    // strip must not be refused by the pre-decode guard.
    records.clear();
    static_cast<void>(atom::image::DecodeImageMemory(SpanOf(kThinBmpHeader)));
    ATOM_CHECK(!saw_message("rejected before decode"));

    // Empty input fails closed before any decoder sees it.
    records.clear();
    ATOM_CHECK(!atom::image::DecodeImageMemory({}).IsValid());
    ATOM_CHECK(saw_message("empty or exceeds stb_image limits"));

    // A header that cannot be probed falls through to the ordinary decode
    // failure instead of being reported as an oversized image.
    const std::array<uint8_t, 10> garbage{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    records.clear();
    ATOM_CHECK(!atom::image::DecodeImageMemory(SpanOf(garbage)).IsValid());
    ATOM_CHECK(saw_message("decode failed"));
    ATOM_CHECK(!saw_message("rejected before decode"));

    connection.Reset();
    return 0;
}
