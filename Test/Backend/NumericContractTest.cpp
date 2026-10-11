/**
 * @file           : NumericContractTest.cpp
 * @brief          : ARCH-006 — the shared public numeric boundary contract.
 */

#include <Backend/Contracts/Numeric/NumericContract.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <cmath>
#include <cstddef>
#include <limits>

auto main() -> int {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    // --- volume -------------------------------------------------------------
    ATOM_CHECK(atom::numeric::IsValidVolume(0.0f));
    ATOM_CHECK(atom::numeric::IsValidVolume(100.0f));
    ATOM_CHECK(!atom::numeric::IsValidVolume(-0.5f));
    ATOM_CHECK(!atom::numeric::IsValidVolume(100.5f));
    ATOM_CHECK(!atom::numeric::IsValidVolume(nan));
    ATOM_CHECK(!atom::numeric::IsValidVolume(inf));

    ATOM_CHECK_NEAR(atom::numeric::SanitizeVolume(42.0f), 42.0f, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeVolume(-10.0f), atom::numeric::min_volume, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeVolume(250.0f), atom::numeric::max_volume, 0.0f);
    // NaN/Inf must collapse to silence, never propagate into a gain.
    ATOM_CHECK_NEAR(atom::numeric::SanitizeVolume(nan), atom::numeric::min_volume, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeVolume(inf), atom::numeric::min_volume, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeVolume(-inf), atom::numeric::min_volume, 0.0f);

    // --- radius -------------------------------------------------------------
    ATOM_CHECK(atom::numeric::IsValidRadius(0.5f));
    ATOM_CHECK(atom::numeric::IsValidRadius(atom::numeric::max_radius));
    ATOM_CHECK(!atom::numeric::IsValidRadius(0.0f));
    ATOM_CHECK(!atom::numeric::IsValidRadius(-1.0f));
    ATOM_CHECK(!atom::numeric::IsValidRadius(nan));
    ATOM_CHECK(!atom::numeric::IsValidRadius(atom::numeric::max_radius * 2.0f));

    ATOM_CHECK_NEAR(atom::numeric::SanitizeRadius(0.0f), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeRadius(-4.0f), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeRadius(nan), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeRadius(atom::numeric::max_radius * 4.0f), atom::numeric::max_radius,
                    0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeRadius(12.0f), 12.0f, 0.0f);

    // --- seconds / playback offsets ----------------------------------------
    float seconds = -1.0f;
    ATOM_CHECK(!atom::numeric::TrySanitizeOffsetSeconds(nan, seconds));
    ATOM_CHECK(!atom::numeric::TrySanitizeOffsetSeconds(inf, seconds));
    ATOM_CHECK(seconds == -1.0f); // a rejected request leaves the output untouched

    ATOM_CHECK(atom::numeric::TrySanitizeOffsetSeconds(-5.0f, seconds));
    ATOM_CHECK_NEAR(seconds, 0.0f, 0.0f);
    ATOM_CHECK(atom::numeric::TrySanitizeOffsetSeconds(12.5f, seconds));
    ATOM_CHECK_NEAR(seconds, 12.5f, 0.0f);
    ATOM_CHECK(atom::numeric::TrySanitizeOffsetSeconds(1.0e12f, seconds));
    ATOM_CHECK_NEAR(seconds, atom::numeric::max_media_seconds, 0.0f);

    ATOM_CHECK_NEAR(atom::numeric::SanitizeSeconds(-1.0f), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeSeconds(nan), 0.0f, 0.0f);
    ATOM_CHECK_NEAR(atom::numeric::SanitizeSeconds(3.0f), 3.0f, 0.0f);

    // --- voice counts -------------------------------------------------------
    ATOM_CHECK(atom::numeric::IsValidVoiceCount(atom::numeric::min_voices));
    ATOM_CHECK(atom::numeric::IsValidVoiceCount(atom::numeric::max_voices));
    ATOM_CHECK(!atom::numeric::IsValidVoiceCount(0));
    ATOM_CHECK(!atom::numeric::IsValidVoiceCount(atom::numeric::max_voices + 1));

    ATOM_CHECK(atom::numeric::SanitizeVoiceCount(0) == atom::numeric::default_voices);
    ATOM_CHECK(atom::numeric::SanitizeVoiceCount(0, 4) == 4);
    ATOM_CHECK(atom::numeric::SanitizeVoiceCount(1) == 1);
    ATOM_CHECK(atom::numeric::SanitizeVoiceCount(1'000'000) == atom::numeric::max_voices);

    // --- allocation sizes ---------------------------------------------------
    std::size_t bytes = 0;
    ATOM_CHECK(atom::numeric::CheckedPixelBytes(16, 16, 4, bytes));
    ATOM_CHECK(bytes == 1024);
    ATOM_CHECK(!atom::numeric::CheckedPixelBytes(0, 16, 4, bytes));
    ATOM_CHECK(!atom::numeric::CheckedPixelBytes(16, 0, 4, bytes));
    // A width*height product that cannot be represented must be refused instead
    // of wrapping into a small allocation. On a 64-bit size_t the two uint32_t
    // dimensions cannot overflow the product, so it is the per-pixel term that
    // rejects this case; both guards exist for the 32-bit build.
    ATOM_CHECK(!atom::numeric::CheckedPixelBytes(0xFFFFFFFFu, 0xFFFFFFFFu, 4, bytes));

    ATOM_CHECK(atom::numeric::IsValidTextureSize(1, 1));
    ATOM_CHECK(atom::numeric::IsValidTextureSize(atom::numeric::max_texture_dimension,
                                                 atom::numeric::max_texture_dimension));
    ATOM_CHECK(!atom::numeric::IsValidTextureSize(0, 8));
    ATOM_CHECK(!atom::numeric::IsValidTextureSize(8, 0));
    ATOM_CHECK(!atom::numeric::IsValidTextureSize(atom::numeric::max_texture_dimension + 1, 8));

    // --- decoded image budget (total surface, not per axis) ----------------
    ATOM_CHECK(atom::numeric::IsWithinDecodedImageBudget(1, 1));
    // A thin strip is cheap even though it is far past the texture cap.
    ATOM_CHECK(atom::numeric::IsWithinDecodedImageBudget(1, 200000));
    ATOM_CHECK(atom::numeric::IsWithinDecodedImageBudget(16384, 4096));
    // ... and a huge square surface is not, even though each axis is fine.
    ATOM_CHECK(!atom::numeric::IsWithinDecodedImageBudget(16384, 16384));
    ATOM_CHECK(!atom::numeric::IsWithinDecodedImageBudget(20000, 20000));
    ATOM_CHECK(!atom::numeric::IsWithinDecodedImageBudget(0, 16));
    ATOM_CHECK(!atom::numeric::IsWithinDecodedImageBudget(16, 0));

    // --- borrowed buffers ---------------------------------------------------
    const char payload = 'x';
    ATOM_CHECK(atom::numeric::IsValidBuffer(&payload, 1));
    ATOM_CHECK(!atom::numeric::IsValidBuffer(nullptr, 8));
    ATOM_CHECK(!atom::numeric::IsValidBuffer(&payload, 0));
    ATOM_CHECK(!atom::numeric::IsValidBuffer(nullptr, 0));

    return 0;
}
