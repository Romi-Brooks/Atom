/**
 * @file           : ResourceManagerAsyncTest.cpp
 * @brief          : Contract tests for ResourceManager::AcquireAsync / PumpAsync
 *                   (ARCH-107). Covers the pull-model future contract:
 *
 *   - AcquireAsync returns immediately with a Pending future; the future only
 *     becomes terminal when PumpAsync runs (cache insertion is render-thread-
 *     only), so "returned" never means "ready".
 *   - the cached fast path completes synchronously (no PumpAsync needed), and a
 *     type mismatch on a cached id fails the future immediately.
 *   - the inline fallback (job system not running) decodes synchronously but
 *     still finalizes through PumpAsync.
 *   - concurrent acquires of one id share a single in-flight load.
 *   - Evict cancels an in-flight load so an evicted resource is not resurrected,
 *     surfacing as FutureState::Cancelled (and FutureCancelled from Get()).
 *   - a loader failure propagates as a Failed future carrying the loader's own
 *     reason (FsNotFound).
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
#include <thread>
#include <vector>

namespace {

auto Bytes(const std::string_view text) -> std::vector<std::byte> {
    std::vector<std::byte> bytes(text.size());
    for (std::size_t index = 0; index < text.size(); ++index)
        bytes[index] = static_cast<std::byte>(text[index]);
    return bytes;
}

// Reads a file's whole content into a std::string. Stateless, so it is safe to
// run from a worker.
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

// Blocks on a gate until the test releases it, then produces a value and counts
// how many times Load actually ran. This lets the test hold a load in-flight to
// observe Pending and cancel semantics, and to prove duplicate ids share one
// load (Count() == 1 after two concurrent acquires).
class GatedIntLoader final : public atom::asset::TypedResourceLoader<int> {
    public:
        [[nodiscard]] auto Kind() const -> atom::asset::AssetKind override {
            return atom::asset::AssetKind::Script;
        }

        [[nodiscard]] auto Count() const -> int {
            return count_.load(std::memory_order_acquire);
        }

        auto Release() -> void {
            gate_.store(true, std::memory_order_release);
        }

    protected:
        [[nodiscard]] auto LoadTyped(const atom::fs::IFileSystem&, const atom::asset::ResourceId&,
                                     std::shared_ptr<int>& output) -> atom::Result<void> override {
            while (!gate_.load(std::memory_order_acquire))
                std::this_thread::yield();
            const int value = count_.fetch_add(1, std::memory_order_relaxed) + 1;
            output = std::make_shared<int>(value);
            return atom::Ok();
        }

    private:
        std::atomic<bool> gate_{false};
        std::atomic<int> count_{0};
};

} // namespace

auto main() -> int {
    std::unique_ptr<atom::fs::MemoryFileSystem> memory{};
    if (atom::fs::MemoryFileSystem::Create("res", memory) != atom::fs::Result::Success || !memory)
        return atom::test::Fail("Could not create memory filesystem");

    atom::fs::AssetPath config_path{};
    atom::fs::AssetPath missing_path{};
    atom::fs::AssetPath script_path{};
    if (!atom::fs::AssetPath::TryParse("res://config/default.txt", config_path) ||
        !atom::fs::AssetPath::TryParse("res://config/missing.txt", missing_path) ||
        !atom::fs::AssetPath::TryParse("res://scripts/main.lua", script_path))
        return atom::test::Fail("Could not parse asset paths");
    if (memory->WriteFile(config_path, Bytes("hello-atom")) != atom::fs::Result::Success)
        return atom::test::Fail("Could not seed memory file");

    atom::asset::ResourceId config_id{};
    atom::asset::ResourceId missing_id{};
    atom::asset::ResourceId script_id{};
    if (!atom::asset::ResourceId::TryCreate(config_path, atom::asset::AssetKind::Config, "", config_id) ||
        !atom::asset::ResourceId::TryCreate(missing_path, atom::asset::AssetKind::Config, "", missing_id) ||
        !atom::asset::ResourceId::TryCreate(script_path, atom::asset::AssetKind::Script, "", script_id))
        return atom::test::Fail("Could not create resource ids");

    atom::asset::ResourceManager manager{*memory};
    if (!manager.RegisterLoader(std::make_shared<StringConfigLoader>()))
        return atom::test::Fail("Could not register config loader");

    // Drain a future to terminal by pumping on the render thread, mirroring the
    // per-frame PumpAsync call. Bounded so a bug that leaves a future Pending
    // fails the test instead of hanging it.
    const auto pump_until_ready = [&manager](auto& future, const int max_iterations = 1'000'000) -> bool {
        for (int i = 0; i < max_iterations && !future.IsReady(); ++i)
            manager.PumpAsync();
        return future.IsReady();
    };

    // --- Phase A: job system not running → inline fallback ------------------
    // AcquireAsync must return a Pending future even though decoding happened
    // inline: the value handoff is PumpAsync's job, never the worker's.
    {
        auto& jobs = atom::job::JobSystem::GetInstance();
        if (jobs.IsRunning())
            jobs.Shutdown();
        ATOM_CHECK(!jobs.IsRunning());

        auto future = manager.AcquireAsync<std::string>(config_id);
        ATOM_CHECK(future.IsReady() == false); // Pending until PumpAsync
        manager.PumpAsync();
        ATOM_CHECK(future.IsReady());
        ATOM_CHECK(future.State() == atom::async::FutureState::Ready);

        const auto result = future.Get();
        ATOM_CHECK(result.has_value());
        const auto handle = result.value();
        ATOM_CHECK(handle.IsValid());
        ATOM_CHECK(*handle == "hello-atom");
    }

    // --- Phase B: cached fast path + type mismatch --------------------------
    // A cached id resolves synchronously (already Ready, no PumpAsync), and a
    // type mismatch on that cached id fails the future immediately. The cache
    // holds a weak reference, so a live handle must outlive the check.
    {
        auto live = manager.Acquire<std::string>(config_id);
        ATOM_CHECK(live.IsValid());

        auto hit = manager.AcquireAsync<std::string>(config_id);
        ATOM_CHECK(hit.IsReady());
        ATOM_CHECK(hit.Get().has_value());
        ATOM_CHECK(*hit.Get().value() == "hello-atom");

        auto mismatch = manager.AcquireAsync<int>(config_id);
        ATOM_CHECK(mismatch.IsReady());
        ATOM_CHECK(mismatch.State() == atom::async::FutureState::Failed);
        ATOM_CHECK(mismatch.Get().error().Code() == atom::ErrorCode::AssetTypeMismatch);
    }

    // --- Phase C: inline fallback surfaces the loader's own reason ----------
    {
        auto future = manager.AcquireAsync<std::string>(missing_id);
        ATOM_CHECK(future.IsReady() == false);
        manager.PumpAsync();
        ATOM_CHECK(future.IsReady());
        ATOM_CHECK(future.State() == atom::async::FutureState::Failed);
        ATOM_CHECK(future.Get().error().Code() == atom::ErrorCode::FsNotFound);
    }

    // --- Phase D: running scheduler — pending, then shared in-flight load ---
    {
        manager.EvictAll();

        auto gated = std::make_shared<GatedIntLoader>();
        if (!manager.RegisterLoader(gated))
            return atom::test::Fail("Could not register gated script loader");

        auto& jobs = atom::job::JobSystem::GetInstance();
        jobs.Configure(4);
        ATOM_CHECK(jobs.Start());
        ATOM_CHECK(jobs.IsRunning());

        auto first = manager.AcquireAsync<int>(script_id);
        auto second = manager.AcquireAsync<int>(script_id);

        // Both must be Pending now, and even a PumpAsync pass must not complete
        // them: the worker is gated and has not written the raw resource yet.
        ATOM_CHECK(first.IsReady() == false);
        ATOM_CHECK(second.IsReady() == false);
        manager.PumpAsync();
        ATOM_CHECK(first.IsReady() == false);

        gated->Release();
        ATOM_CHECK(pump_until_ready(first));
        ATOM_CHECK(second.IsReady());

        const auto a = first.Get();
        const auto b = second.Get();
        ATOM_CHECK(a.has_value() && b.has_value());
        ATOM_CHECK(a.value().IsValid() && b.value().IsValid());
        // Duplicate ids share one load: the loader ran exactly once.
        ATOM_CHECK(gated->Count() == 1);
        ATOM_CHECK(a.value().Get() == b.value().Get());

        jobs.Shutdown();
    }

    // --- Phase E: Evict cancels an in-flight load ---------------------------
    {
        manager.EvictAll();

        auto gated = std::make_shared<GatedIntLoader>();
        if (!manager.RegisterLoader(gated))
            return atom::test::Fail("Could not register gated script loader (cancel)");

        auto& jobs = atom::job::JobSystem::GetInstance();
        jobs.Configure(4);
        ATOM_CHECK(jobs.Start());

        auto future = manager.AcquireAsync<int>(script_id);
        ATOM_CHECK(future.IsReady() == false);

        // Evicting a resource whose load is still in flight cancels that load,
        // so the pending future resolves as Cancelled instead of resurrecting
        // the evicted entry later. The id was never cached (only in flight), so
        // Evict reports false while still cancelling the pending load.
        ATOM_CHECK(manager.Evict(script_id) == false);
        ATOM_CHECK(future.IsReady());
        ATOM_CHECK(future.State() == atom::async::FutureState::Cancelled);
        ATOM_CHECK(future.Get().error().Code() == atom::ErrorCode::FutureCancelled);

        // Release the worker so it can finish harmlessly: it observes the
        // cancelled flag and drops the raw result, which nothing reads again.
        gated->Release();

        jobs.Shutdown();
    }

    return 0;
}
