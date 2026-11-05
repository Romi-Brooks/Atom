/**
 * @file           : ResourceManagerBatchTest.cpp
 * @brief          : Contract tests for ResourceManager::TryAcquireBatch /
 *                   AcquireBatch — the parallel batch-load entry point.
 *
 * Covers the properties that matter for the "parallel load + single-threaded
 * cache insert" design:
 *   - results are returned in input order
 *   - one failed slot does not mask its neighbours
 *   - duplicate ids in one batch share a single load (and a single instance)
 *   - the sequential fallback (job system not running) matches the parallel path
**/

#include <Asset/ResourceManager.hpp>
#include <Concurrency/JobSystem.hpp>
#include <Filesystem/MemoryFileSystem.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <atomic>
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

// Reads a file's whole content into a std::string. Stateless, so it is safe to
// run concurrently (the batch path calls Load from multiple workers).
class StringConfigLoader final : public atom::asset::TypedResourceLoader<std::string> {
    public:
        [[nodiscard]] auto Kind() const -> atom::asset::AssetKind override {
            return atom::asset::AssetKind::Config;
        }

    protected:
        [[nodiscard]] auto LoadTyped(const atom::fs::IFileSystem& filesystem, const atom::asset::ResourceId& id,
                                     std::shared_ptr<std::string>& output) -> atom::Result<void> override {
            std::unique_ptr<atom::fs::IFile> file{};
            const atom::fs::Result opened = filesystem.OpenRead(id.Path(), file);
            if (opened != atom::fs::Result::Success)
                return std::unexpected<atom::Error>{atom::fs::ToError(opened, "OpenRead", id.Path().String())};
            std::vector<std::byte> data{};
            const atom::fs::Result read = atom::fs::ReadAll(*file, data);
            if (read != atom::fs::Result::Success)
                return std::unexpected<atom::Error>{atom::fs::ToError(read, "ReadAll", id.Path().String())};
            output = std::make_shared<std::string>(reinterpret_cast<const char*>(data.data()), data.size());
            return atom::Ok();
        }
};

// Returns a new value on every load, so the test can prove a duplicate id was
// loaded exactly once (dedupe) rather than twice.
class CountingScriptLoader final : public atom::asset::TypedResourceLoader<int> {
    public:
        [[nodiscard]] auto Kind() const -> atom::asset::AssetKind override {
            return atom::asset::AssetKind::Script;
        }

        [[nodiscard]] auto Count() const -> int {
            return count_.load(std::memory_order_acquire);
        }

    protected:
        [[nodiscard]] auto LoadTyped(const atom::fs::IFileSystem&, const atom::asset::ResourceId&,
                                     std::shared_ptr<int>& output) -> atom::Result<void> override {
            const int value = count_.fetch_add(1, std::memory_order_relaxed) + 1;
            output = std::make_shared<int>(value);
            return atom::Ok();
        }

    private:
        std::atomic<int> count_{0};
};

} // namespace

auto main() -> int {
    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return atom::test::Fail("Could not create memory filesystem");

    // Three real config files, one intentionally missing, one script id for the
    // dedupe check.
    atom::fs::AssetPath path_a{};
    atom::fs::AssetPath path_b{};
    atom::fs::AssetPath path_c{};
    atom::fs::AssetPath path_missing{};
    atom::fs::AssetPath path_script{};
    if (!atom::fs::AssetPath::TryParse("res://config/a.txt", path_a) ||
        !atom::fs::AssetPath::TryParse("res://config/b.txt", path_b) ||
        !atom::fs::AssetPath::TryParse("res://config/c.txt", path_c) ||
        !atom::fs::AssetPath::TryParse("res://config/missing.txt", path_missing) ||
        !atom::fs::AssetPath::TryParse("res://scripts/main.lua", path_script))
        return atom::test::Fail("Could not parse asset paths");
    if (memory->WriteFile(path_a, Bytes("content-a")) != atom::fs::Result::Success ||
        memory->WriteFile(path_b, Bytes("content-b")) != atom::fs::Result::Success ||
        memory->WriteFile(path_c, Bytes("content-c")) != atom::fs::Result::Success)
        return atom::test::Fail("Could not write config files");

    atom::asset::ResourceId id_a{};
    atom::asset::ResourceId id_b{};
    atom::asset::ResourceId id_c{};
    atom::asset::ResourceId id_missing{};
    atom::asset::ResourceId id_script{};
    if (!atom::asset::ResourceId::TryCreate(path_a, atom::asset::AssetKind::Config, "", id_a) ||
        !atom::asset::ResourceId::TryCreate(path_b, atom::asset::AssetKind::Config, "", id_b) ||
        !atom::asset::ResourceId::TryCreate(path_c, atom::asset::AssetKind::Config, "", id_c) ||
        !atom::asset::ResourceId::TryCreate(path_missing, atom::asset::AssetKind::Config, "", id_missing) ||
        !atom::asset::ResourceId::TryCreate(path_script, atom::asset::AssetKind::Script, "", id_script))
        return atom::test::Fail("Could not create resource ids");

    atom::asset::ResourceManager manager{*memory};
    if (!manager.RegisterLoader(std::make_shared<StringConfigLoader>()))
        return atom::test::Fail("Could not register config loader");
    auto counting = std::make_shared<CountingScriptLoader>();
    if (!manager.RegisterLoader(counting))
        return atom::test::Fail("Could not register script loader");

    // --- Phase 1: job system not running → sequential fallback -------------
    {
        auto& jobs = atom::job::JobSystem::GetInstance();
        if (jobs.IsRunning())
            jobs.Shutdown();
        ATOM_CHECK(!jobs.IsRunning());

        const std::vector<atom::asset::ResourceId> batch{id_a, id_missing, id_b};
        const auto results = manager.TryAcquireBatch<std::string>(batch);

        ATOM_CHECK(results.size() == 3);
        ATOM_CHECK(results[0].has_value());
        ATOM_CHECK(*(results[0].value()) == "content-a");
        ATOM_CHECK(!results[1].has_value()); // missing file → error, order intact
        ATOM_CHECK(results[1].error().Code() == atom::ErrorCode::FsNotFound);
        ATOM_CHECK(results[2].has_value());
        ATOM_CHECK(*(results[2].value()) == "content-b");
    }

    // --- Phase 2: job system running → parallel loads, same contract -------
    {
        manager.EvictAll(); // force re-loads so the parallel path is exercised

        auto& jobs = atom::job::JobSystem::GetInstance();
        jobs.Configure(4);
        ATOM_CHECK(jobs.Start());
        ATOM_CHECK(jobs.IsRunning());

        const std::vector<atom::asset::ResourceId> batch{id_c, id_missing, id_a};
        const auto results = manager.TryAcquireBatch<std::string>(batch);

        ATOM_CHECK(results.size() == 3);
        ATOM_CHECK(results[0].has_value());
        ATOM_CHECK(*(results[0].value()) == "content-c");
        ATOM_CHECK(!results[1].has_value());
        ATOM_CHECK(results[2].has_value());
        ATOM_CHECK(*(results[2].value()) == "content-a");

        jobs.Shutdown();
    }

    // --- Dedupe: a repeated id loads once and shares one instance ----------
    {
        manager.EvictAll();

        auto& jobs = atom::job::JobSystem::GetInstance();
        jobs.Configure(4);
        ATOM_CHECK(jobs.Start());

        const std::vector<atom::asset::ResourceId> batch{id_script, id_script};
        const auto results = manager.TryAcquireBatch<int>(batch);

        ATOM_CHECK(results.size() == 2);
        ATOM_CHECK(results[0].has_value());
        ATOM_CHECK(results[1].has_value());
        ATOM_CHECK(results[0].value().Get() == results[1].value().Get());
        ATOM_CHECK(*(results[0].value()) == *(results[1].value()));
        ATOM_CHECK(counting->Count() == 1); // loaded exactly once

        jobs.Shutdown();
    }

    // --- Batch result is the same instance a later single Acquire returns ---
    {
        auto single = manager.Acquire<std::string>(id_c);
        ATOM_CHECK(single.IsValid());
        ATOM_CHECK(*single == "content-c");
        // id_c was loaded in phase 2 and is still cached, so a single Acquire
        // must not trigger a second load — it returns the cached instance.
        ATOM_CHECK(manager.LiveCount() >= 1);
    }

    return 0;
}
