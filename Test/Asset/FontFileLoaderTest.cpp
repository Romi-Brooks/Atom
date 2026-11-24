/**
 * @file           : FontFileLoaderTest.cpp
 * @brief          : Contract tests for FontFileLoader (ARCH-107 P4): font bytes
 *                   enter the ResourceManager URI/identity + async-acquire
 *                   machinery just like textures and audio.
 *
 * Covers:
 *   - synchronous TryAcquire<FontData> returns the exact file bytes
 *   - AcquireAsync<FontData> + PumpAsync loads off-thread and yields the bytes
 *   - the cached fast path resolves immediately
 *   - an empty font file fails with AssetLoadFailed; a missing file forwards
 *     the filesystem's own reason (FsNotFound)
**/

#include <Asset/ResourceManager.hpp>
#include <Concurrency/JobSystem.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Render/Text/FontFileLoader.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

} // namespace

auto main() -> int {
    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return atom::test::Fail("Could not create memory filesystem");

    atom::fs::AssetPath font_path{};
    atom::fs::AssetPath missing_path{};
    atom::fs::AssetPath empty_path{};
    if (!atom::fs::AssetPath::TryParse("res://fonts/test.ttf", font_path) ||
        !atom::fs::AssetPath::TryParse("res://fonts/missing.ttf", missing_path) ||
        !atom::fs::AssetPath::TryParse("res://fonts/empty.ttf", empty_path))
        return atom::test::Fail("Could not parse font paths");

    const auto payload = Bytes("fake-font-bytes-for-loader-test");
    if (memory->WriteFile(font_path, payload) != atom::fs::Result::Success)
        return atom::test::Fail("Could not write font file");
    if (memory->WriteFile(empty_path, {}) != atom::fs::Result::Success)
        return atom::test::Fail("Could not write empty font file");

    atom::asset::ResourceId font_id{};
    atom::asset::ResourceId missing_id{};
    atom::asset::ResourceId empty_id{};
    if (!atom::asset::ResourceId::TryCreate(font_path, atom::asset::AssetKind::Font, "", font_id) ||
        !atom::asset::ResourceId::TryCreate(missing_path, atom::asset::AssetKind::Font, "", missing_id) ||
        !atom::asset::ResourceId::TryCreate(empty_path, atom::asset::AssetKind::Font, "", empty_id))
        return atom::test::Fail("Could not create font resource ids");

    atom::asset::ResourceManager manager{*memory};
    if (!manager.RegisterLoader(std::make_shared<atom::render::FontFileLoader>()))
        return atom::test::Fail("Could not register FontFileLoader");

    // --- Synchronous load returns the exact bytes ---------------------------
    {
        const auto result = manager.TryAcquire<atom::render::FontData>(font_id);
        ATOM_CHECK(result.has_value());
        const auto handle = result.value();
        ATOM_CHECK(handle.IsValid());
        const auto* data = handle.Get();
        ATOM_CHECK(data != nullptr);
        ATOM_CHECK(data->bytes.size() == payload.size());
        ATOM_CHECK(std::equal(data->bytes.begin(), data->bytes.end(), payload.begin(), payload.end()));
    }

    // --- Cached fast path resolves immediately ------------------------------
    {
        auto live = manager.Acquire<atom::render::FontData>(font_id);
        ATOM_CHECK(live.IsValid());

        auto future = manager.AcquireAsync<atom::render::FontData>(font_id);
        ATOM_CHECK(future.IsReady());
        ATOM_CHECK(future.Get().has_value());
        ATOM_CHECK(future.Get().value().Get() == live.Get());
    }

    // --- Async load (job system on): off-thread read + PumpAsync finalize ---
    {
        // Drop the cached entry so the worker path runs (it was cached by the
        // synchronous and fast-path checks above, so Evict reports true).
        ATOM_CHECK(manager.Evict(font_id) == true);

        auto& jobs = atom::job::JobSystem::GetInstance();
        jobs.Configure(2);
        ATOM_CHECK(jobs.Start());

        auto future = manager.AcquireAsync<atom::render::FontData>(font_id);
        ATOM_CHECK(future.IsReady() == false); // Pending until PumpAsync
        for (int i = 0; i < 1'000'000 && !future.IsReady(); ++i)
            manager.PumpAsync();
        ATOM_CHECK(future.IsReady());

        const auto result = future.Get();
        ATOM_CHECK(result.has_value());
        const auto handle = result.value();
        ATOM_CHECK(handle.IsValid());
        ATOM_CHECK(handle.Get()->bytes.size() == payload.size());
        ATOM_CHECK(std::equal(handle.Get()->bytes.begin(), handle.Get()->bytes.end(), payload.begin(), payload.end()));

        jobs.Shutdown();
    }

    // --- Failure: empty file and missing file --------------------------------
    {
        const auto empty = manager.TryAcquire<atom::render::FontData>(empty_id);
        ATOM_CHECK(!empty.has_value());
        ATOM_CHECK(empty.error().Code() == atom::ErrorCode::AssetLoadFailed);

        const auto missing = manager.TryAcquire<atom::render::FontData>(missing_id);
        ATOM_CHECK(!missing.has_value());
        ATOM_CHECK(missing.error().Code() == atom::ErrorCode::FsNotFound);
    }

    return 0;
}
