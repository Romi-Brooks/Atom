/**
 * @file           : JobSystemTour.cpp
 * @brief          : A self-contained tour of the Atom job system. Walks through
 *                   the core API step by step — schedule, dependencies,
 *                   priorities, main-thread helping, nested submission, and the
 *                   diagnostics snapshot — each printed to the console so the
 *                   reader can follow along without reading any engine source.
 *
 * Run it from the command line; every section is independent and prints its own
 * result, so the whole file doubles as a readable tutorial.
 *
 * Copyright (c) 2026 Romi Brooks, All rights reserved.
 */

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <Concurrency/JobSystem.hpp>

namespace {

using atom::job::JobHandle;
using atom::job::JobSystem;
using atom::job::Priority;

// A tiny section header so the console output reads like a guided tour.
auto Header(const std::string& title) -> void {
    std::cout << "\n==== " << title << " " << std::string(48 - std::min<std::size_t>(48, title.size()), '=')
              << "\n";
}

} // namespace

auto main() -> int {
    // One scheduler for the whole tour. Configure() picks
    // hardware_concurrency() - 1 workers (>= 1); Start() spawns them.
    auto& jobs = JobSystem::GetInstance();
    jobs.Configure();
    jobs.Start();
    std::cout << "Job system running with " << jobs.WorkerCount() << " worker(s).\n";

    // --- 1. A single job: schedule and wait ---------------------------------
    Header("1. A single job");
    {
        std::atomic<int> answer{0};
        auto handle = jobs.Schedule([&answer] { answer.store(42); });
        jobs.WaitFor(handle);
        std::cout << "Scheduled one job; after WaitFor, answer = " << answer.load() << "\n";
    }

    // --- 2. Dependencies: B runs only after A ------------------------------
    Header("2. Dependency chain (A -> B)");
    {
        std::atomic<int> order{0};
        // A increments, B depends on A and increments again. B must observe
        // A's write, proving the dependency was honoured.
        auto a = jobs.Schedule([&order] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            order.fetch_add(1);
        });
        auto b = jobs.Schedule([&order] { order.fetch_add(1); }, a);
        jobs.WaitFor(b);
        std::cout << "order after A then B = " << order.load() << " (expect 2)\n";
    }

    // --- 3. Fan-out parallelism: many independent jobs ---------------------
    Header("3. Parallel increments (1000 jobs)");
    {
        constexpr int job_count = 1000;
        constexpr int per_job = 100;
        std::atomic<int> total{0};
        std::vector<JobHandle> handles;
        handles.reserve(job_count);
        for (int i = 0; i < job_count; ++i)
            handles.push_back(jobs.Schedule([&total] {
                for (int k = 0; k < per_job; ++k)
                    total.fetch_add(1);
            }));
        jobs.WaitAll(handles);
        std::cout << "total = " << total.load() << " (expect " << job_count * per_job << ")\n";
    }

    // --- 4. Priority: High jumps ahead of queued Normal --------------------
    Header("4. Priority (High before queued Normal)");
    {
        const auto workers = jobs.WorkerCount();
        std::atomic<int> running{0};
        std::atomic<int> sequence{0};
        std::atomic<int> high_at{-1};
        std::atomic<int> normal_at{-1};

        // Occupy every worker with a brief blocking job.
        std::vector<JobHandle> blockers;
        blockers.reserve(workers);
        for (std::size_t i = 0; i < workers; ++i)
            blockers.push_back(jobs.Schedule(
                [&running] {
                    running.fetch_add(1);
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                },
                Priority::Normal));
        while (running.load() < static_cast<int>(workers))
            std::this_thread::yield();

        // Now submit a High and a Normal job while all workers are busy.
        auto high = jobs.Schedule([&sequence, &high_at] { high_at.store(sequence.fetch_add(1)); }, Priority::High);
        auto normal =
            jobs.Schedule([&sequence, &normal_at] { normal_at.store(sequence.fetch_add(1)); }, Priority::Normal);

        jobs.WaitAll({high, normal});
        jobs.WaitAll(blockers);
        std::cout << "High ran at sequence " << high_at.load() << ", Normal at " << normal_at.load()
                  << " (High must be first)\n";
    }

    // --- 5. Main-thread helping --------------------------------------------
    Header("5. Main thread helps while waiting");
    {
        // A dependency chain that has not started yet; WaitFor keeps the main
        // thread productive by running pending jobs instead of blocking.
        std::atomic<int> value{0};
        auto a = jobs.Schedule([&value] { value.fetch_add(1); });
        auto b = jobs.Schedule([&value] { value.fetch_add(10); }, a);
        jobs.WaitFor(b);
        std::cout << "value = " << value.load() << " (expect 11)\n";
    }

    // --- 6. Nested submission from inside a job ----------------------------
    Header("6. Nested submission (a job schedules children)");
    {
        std::atomic<int> children{0};
        auto root = jobs.Schedule([&jobs, &children] {
            std::vector<JobHandle> kids;
            for (int i = 0; i < 50; ++i)
                kids.push_back(jobs.Schedule([&children] { children.fetch_add(1); }));
            jobs.WaitAll(kids);
        });
        jobs.WaitFor(root);
        std::cout << "children completed = " << children.load() << " (expect 50)\n";
    }

    // --- 7. Diagnostics snapshot -------------------------------------------
    Header("7. Diagnostics snapshot (Stats)");
    {
        const auto stats = jobs.Stats();
        std::cout << "submitted=" << stats.submitted << " completed=" << stats.completed
                  << " pending=" << stats.pending << " stolen=" << stats.stolen << "\n";
    }

    jobs.Shutdown();
    std::cout << "\nTour finished. The scheduler is shut down and all workers joined.\n";
    return 0;
}
