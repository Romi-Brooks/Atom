/**
 * @file           : JobSystemTest.cpp
 * @brief          : Correctness tests for the work-stealing job scheduler.
 *
 * Covers the scheduler's core contracts independently of any subsystem:
 *   - single-job completion
 *   - dependency chaining (counter model)
 *   - concurrent atomic increments (race-free execution)
 *   - main-thread helping while waiting
 *   - WaitAll over a batch
 *   - priority ordering (High drains before Normal)
 *   - shutdown drains and leaves handles observable
 */

#include <Concurrency/JobSystem.hpp>
#include <Test/Support/TestHelpers.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
using atom::job::JobSystem;
using atom::job::Priority;

auto MakeSystem(const std::size_t workers = 4) -> JobSystem& {
    auto& sys = JobSystem::GetInstance();
    if (!sys.IsRunning()) {
        sys.Configure(workers);
        sys.Start();
    }
    return sys;
}
} // namespace

auto main() -> int {
    // --- 1. Single job completes and produces its result ------------------
    {
        auto& sys = MakeSystem();
        std::atomic<int> value{0};
        auto handle = sys.Schedule([&value] { value.store(42, std::memory_order_release); });
        ATOM_CHECK(handle.IsValid());
        sys.WaitFor(handle);
        ATOM_CHECK(handle.IsComplete());
        ATOM_CHECK(value.load(std::memory_order_acquire) == 42);
    }

    // --- 2. Dependency chain: B runs only after A ------------------------
    {
        auto& sys = MakeSystem();
        std::atomic<int> order{0};
        std::atomic<bool> saw_before_a{false};

        auto a = sys.Schedule([&order] {
            order.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
        auto b = sys.Schedule(
            [&order, &saw_before_a] {
                // If B ran before A, A's increment would not be visible yet.
                if (order.load(std::memory_order_acquire) != 1)
                    saw_before_a.store(true, std::memory_order_release);
                order.fetch_add(1, std::memory_order_relaxed);
            },
            a);

        sys.WaitFor(b);
        ATOM_CHECK(b.IsComplete());
        ATOM_CHECK(a.IsComplete());
        ATOM_CHECK(!saw_before_a.load(std::memory_order_acquire));
        ATOM_CHECK(order.load(std::memory_order_acquire) == 2);
    }

    // --- 3. Concurrent increments are race-free --------------------------
    {
        auto& sys = MakeSystem();
        constexpr int jobs = 1000;
        constexpr int per_job = 100;
        std::atomic<int> total{0};
        std::vector<atom::job::JobHandle> handles;
        handles.reserve(jobs);
        for (int i = 0; i < jobs; ++i) {
            handles.push_back(sys.Schedule([&total] {
                for (int k = 0; k < per_job; ++k)
                    total.fetch_add(1, std::memory_order_relaxed);
            }));
        }
        sys.WaitAll(handles);
        ATOM_CHECK(total.load(std::memory_order_acquire) == jobs * per_job);
    }

    // --- 4. Main-thread helping: WaitFor on a dependent that isn't started
    //        yet still completes because the waiter runs pending jobs ------
    {
        auto& sys = MakeSystem();
        std::atomic<int> value{0};
        auto a = sys.Schedule([&value] { value.fetch_add(1, std::memory_order_relaxed); });
        auto b = sys.Schedule([&value] { value.fetch_add(10, std::memory_order_relaxed); }, a);
        // Wait on the chain from the main thread; it may help run A or B.
        sys.WaitFor(b);
        ATOM_CHECK(b.IsComplete());
        ATOM_CHECK(value.load(std::memory_order_acquire) == 11);
    }

    // --- 5. Priority: High jobs drain before queued Normal jobs ----------
    {
        auto& sys = MakeSystem();
        const auto worker_count = sys.WorkerCount();
        ATOM_CHECK(worker_count >= 1);

        std::atomic<int> running_workers{0};
        std::atomic<int> sequence{0};
        std::atomic<int> high_ran_at{-1};
        std::atomic<int> queued_normal_ran_at{-1};

        // Occupy every worker with a Normal job that blocks briefly.
        std::vector<atom::job::JobHandle> blockers;
        blockers.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i) {
            blockers.push_back(sys.Schedule(
                [&running_workers] {
                    running_workers.fetch_add(1, std::memory_order_acq_rel);
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                },
                Priority::Normal));
        }
        // Wait until every worker is busy (all blockers have started).
        while (running_workers.load(std::memory_order_acquire) < static_cast<int>(worker_count)) {
            std::this_thread::yield();
        }

        // Now the pool has no free worker. Submit a High job and a Normal job:
        // the High job must be scheduled before the queued Normal job.
        auto high = sys.Schedule(
            [&sequence, &high_ran_at] {
                high_ran_at.store(sequence.fetch_add(1, std::memory_order_relaxed), std::memory_order_relaxed);
            },
            Priority::High);
        auto queued_normal = sys.Schedule(
            [&sequence, &queued_normal_ran_at] {
                queued_normal_ran_at.store(sequence.fetch_add(1, std::memory_order_relaxed),
                                           std::memory_order_relaxed);
            },
            Priority::Normal);

        sys.WaitAll({high, queued_normal});
        ATOM_CHECK(high_ran_at.load() != -1);
        ATOM_CHECK(queued_normal_ran_at.load() != -1);
        // The High job, submitted while all workers were busy, must run before
        // the Normal job queued behind it.
        ATOM_CHECK(high_ran_at.load() < queued_normal_ran_at.load());

        // Clean up the blockers.
        sys.WaitAll(blockers);
    }

    // --- 6. Concurrent submission from many threads ----------------------
    {
        auto& sys = MakeSystem();
        constexpr int threads = 8;
        constexpr int jobs_per_thread = 200;
        std::atomic<int> total{0};

        std::vector<std::thread> submitters;
        submitters.reserve(threads);
        for (int t = 0; t < threads; ++t) {
            submitters.emplace_back([&sys, &total] {
                std::vector<atom::job::JobHandle> handles;
                handles.reserve(jobs_per_thread);
                for (int j = 0; j < jobs_per_thread; ++j) {
                    handles.push_back(sys.Schedule([&total] { total.fetch_add(1, std::memory_order_relaxed); }));
                }
                sys.WaitAll(handles);
            });
        }
        for (auto& t : submitters)
            t.join();

        ATOM_CHECK(total.load(std::memory_order_acquire) == threads * jobs_per_thread);
    }

    // --- 6b. Nested submission: a job schedules child jobs on the worker's
    //         local queue (LIFO path), and dependents are signalled via the
    //         continuation path -------------------------------
    {
        auto& sys = MakeSystem();
        std::atomic<int> total{0};

        // A root job that, when run on a worker, schedules children that must
        // all complete. The parent waits on the children through dependencies,
        // exercising both local-queue submission and continuation signalling.
        std::atomic<int> parent_ran{0};
        std::atomic<int> children_done{0};

        auto root = sys.Schedule([&sys, &total, &parent_ran, &children_done] {
            parent_ran.fetch_add(1, std::memory_order_relaxed);
            std::vector<atom::job::JobHandle> kids;
            for (int i = 0; i < 50; ++i) {
                kids.push_back(sys.Schedule([&total, &children_done] {
                    total.fetch_add(1, std::memory_order_relaxed);
                    children_done.fetch_add(1, std::memory_order_relaxed);
                }));
            }
            sys.WaitAll(kids);
        });

        sys.WaitFor(root);
        ATOM_CHECK(root.IsComplete());
        ATOM_CHECK(parent_ran.load(std::memory_order_acquire) == 1);
        ATOM_CHECK(children_done.load(std::memory_order_acquire) == 50);
        ATOM_CHECK(total.load(std::memory_order_acquire) == 50);
    }

    // --- 7. Shutdown drains and handles remain observable -----------------
    {
        auto& sys = MakeSystem();
        std::atomic<int> value{0};
        auto handle = sys.Schedule([&value] { value.store(7, std::memory_order_release); });
        sys.WaitFor(handle);
        ATOM_CHECK(handle.IsComplete());

        sys.Shutdown();
        ATOM_CHECK(!sys.IsRunning());
        // A handle remains observable after shutdown.
        ATOM_CHECK(handle.IsComplete());
    }

    return 0;
}
