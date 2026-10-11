/**
 * @file           : JobSystem.hpp
 * @brief          : Work-stealing job scheduler over a fixed pool of workers.
 *
 * The Atom job system schedules units of work ("jobs") onto a pool of worker
 * threads. Workers run their own local queue (LIFO) and steal from each other
 * (FIFO) when idle, giving load-balanced, lock-light execution without central
 * dispatch. The submitting thread participates too: WaitFor/WaitAll run other
 * pending jobs while they block, so the main thread never idles waiting for a
 * worker it could be helping.
 *
 * Lifecycle is explicit: Configure() (optional) then Start()/Shutdown().
 * Shutdown() drains every worker and joins them; any handle not yet completed
 * is still valid to observe afterwards.
 */

#ifndef ATOM_CONCURRENCY_JOB_SYSTEM_HPP
#define ATOM_CONCURRENCY_JOB_SYSTEM_HPP

#include <Concurrency/Job.hpp>
#include <Concurrency/WorkStealingDeque.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace atom::job {

class JobSystem {
    public:
        JobSystem();
        ~JobSystem();
        JobSystem(const JobSystem&) = delete;
        auto operator=(const JobSystem&) -> JobSystem& = delete;

        // Process-wide singleton, mirroring Log/TimeSystem access style.
        [[nodiscard]] static auto GetInstance() -> JobSystem&;

        // Optional pre-start tuning. Must be called before Start().
        //   worker_count = 0 means "hardware_concurrency() - 1", clamped to >= 1.
        auto Configure(std::size_t worker_count = 0) -> void;

        // Spawn workers. Idempotent: a second call with a running system is a
        // no-op. Returns false if no worker could be started.
        auto Start() -> bool;

        // Stop accepting work, drain, and join all workers. Safe to call from
        // any thread; a handle waited on after Shutdown is still valid.
        auto Shutdown() -> void;

        [[nodiscard]] auto IsRunning() const noexcept -> bool;
        [[nodiscard]] auto WorkerCount() const noexcept -> std::size_t;

        // Submit a job. `dependencies` may be empty. Returns a handle that can
        // be waited on. Safe to call from any thread, including from inside a
        // running job.
        auto Schedule(std::function<void()> fn, Priority priority = Priority::Normal,
                      std::vector<JobHandle> dependencies = {}) -> JobHandle;

        // Overload taking a single dependency for the common "chain" case.
        auto Schedule(std::function<void()> fn, JobHandle dependency, Priority priority = Priority::Normal) -> JobHandle;

        // Named submission for diagnostics. `name` is attached to the job and
        // surfaced by the debugger panel; it has no effect on scheduling.
        auto Schedule(std::string name, std::function<void()> fn, Priority priority = Priority::Normal,
                      std::vector<JobHandle> dependencies = {}) -> JobHandle;

        // Block until a single job completes, running other pending jobs while
        // waiting (so the calling thread stays productive). Intended for the
        // main/render thread.
        auto WaitFor(const JobHandle& handle) -> void;

        // Block until every handle in `handles` completes, helping run pending
        // jobs while waiting.
        auto WaitAll(const std::vector<JobHandle>& handles) -> void;

        // Number of jobs currently queued or running (approximate, diagnostic).
        [[nodiscard]] auto PendingJobCount() const noexcept -> std::size_t;

        // Approximate throughput snapshot for tooling / a debugger panel.
        [[nodiscard]] auto Stats() const noexcept -> JobStats;

        // A lightweight entry describing a job that is currently executing on
        // some worker. The name is empty for unnamed jobs; the raw pointer is
        // included so a panel can fall back to an address label.
        struct RunningJob {
                const Job* job = nullptr;
                std::string_view name;
        };

        // Snapshot of the jobs currently running, for a debugger panel. The
        // returned names are only valid until the next scheduler mutation, so
        // callers must copy them out immediately (the panel renders synchronously).
        [[nodiscard]] auto GetRunningJobs() const -> std::vector<RunningJob>;

        // One finished job kept for a short while so the debugger panel can
        // show work that completed between two frames (most jobs are far
        // shorter than a frame, so a "running" snapshot alone is almost
        // always empty).
        struct FinishedJob {
                std::string name;      // Copy: the Job is gone by display time.
                double duration_ms = 0.0;
                double finished_at = 0.0; // Steady-clock seconds.
        };

        // Recent completions (bounded ring, newest last), for a debugger panel.
        [[nodiscard]] auto GetRecentJobs() const -> std::vector<FinishedJob>;

    private:
        struct Worker {
            std::jthread thread;
            WorkStealingDeque<std::shared_ptr<Job>>* queue = nullptr; // owned by JobSystem
        };

        // Shared pool of runnable jobs (cross-thread submissions + high-priority).
        // Protected by pool_mutex_ / pool_cv_ (only for the shared queue; workers
        // prefer their own local queues).
        auto PushToPool(std::shared_ptr<Job> job) -> void;
        [[nodiscard]] auto TryPopPool(std::shared_ptr<Job>& out) -> bool;

        // Submit a runnable job, routing it to the calling worker's local queue
        // when called from a worker thread (lock-free) and to the shared pool
        // otherwise. This is the single entry point for "job is now runnable".
        auto SubmitRunnable(std::shared_ptr<Job> job) -> void;

        // Index of the calling thread's worker queue, or -1 when the caller is
        // not a worker (main thread, helper, or an external submitter).
        [[nodiscard]] auto CurrentWorkerIndex() const noexcept -> std::ptrdiff_t;

        // A worker's main loop.
        auto WorkerLoop(std::size_t worker_index, std::stop_token stop) -> void;

        // Run a single job to completion, decrementing its dependents. Returns
        // true if a job was run.
        auto ExecuteOne(std::shared_ptr<Job> job) -> bool;

        // Help the scheduler from a non-worker (main) thread while blocking.
        auto HelpWhileWaiting(const std::function<bool()>& done) -> void;

        // Tries to find and run one job, from own queue, pool, then stealing.
        auto RunOne(std::size_t worker_index) -> bool;

        std::vector<std::unique_ptr<WorkStealingDeque<std::shared_ptr<Job>>>> queues_;
        std::vector<std::unique_ptr<Worker>> workers_;

        std::mutex pool_mutex_;
        std::condition_variable pool_cv_;
        std::vector<std::shared_ptr<Job>> pool_;       // runnable jobs (Normal/Low)
        std::vector<std::shared_ptr<Job>> high_pool_;  // High priority, drained first

        std::atomic<bool> running_{false};
        std::atomic<std::size_t> pending_count_{0};
        std::atomic<std::uint64_t> submitted_count_{0};
        std::atomic<std::uint64_t> completed_count_{0};
        std::atomic<std::uint64_t> stolen_count_{0};
        std::size_t configured_worker_count_ = 0;

        // Jobs currently executing on a worker. Guarded by running_mutex_; a
        // Job* is registered in ExecuteOne() right before the body runs and
        // removed right after. Used only by the debugger panel.
        mutable std::mutex running_mutex_;
        std::vector<const Job*> running_jobs_;

        // Bounded ring of recent completions with durations, guarded by the
        // same mutex. Lets the panel show jobs that finished between frames.
        std::deque<FinishedJob> recent_jobs_;
        static constexpr std::size_t recent_jobs_capacity_ = 64;
};

} // namespace atom::job

#endif // ATOM_CONCURRENCY_JOB_SYSTEM_HPP
