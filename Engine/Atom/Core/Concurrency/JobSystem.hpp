/**
 * @file           : JobSystem.hpp
 * @brief          : Work-stealing job scheduler over a fixed pool of workers.
 *
 * Workers run local jobs, share a priority pool and steal idle work.
 * Shutdown closes admission, drains accepted work and joins the workers.
 */

#ifndef ATOM_CONCURRENCY_JOB_SYSTEM_HPP
#define ATOM_CONCURRENCY_JOB_SYSTEM_HPP

#include <Atom/Core/Concurrency/Job.hpp>
#include <Atom/Core/Concurrency/WorkStealingDeque.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
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

        // Close admission and drain all accepted jobs. A job may request
        // shutdown; an external call joins the workers after it returns.
        auto Shutdown() -> void;

        [[nodiscard]] auto IsRunning() const noexcept -> bool;
        [[nodiscard]] auto WorkerCount() const noexcept -> std::size_t;

        // Submit while running. Dependencies must belong to this scheduler.
        // Rejected submissions return an invalid handle.
        auto Schedule(std::function<void()> fn, Priority priority = Priority::Normal,
                      std::vector<JobHandle> dependencies = {}) -> JobHandle;

        // Overload taking a single dependency for the common "chain" case.
        auto Schedule(std::function<void()> fn, JobHandle dependency, Priority priority = Priority::Normal) -> JobHandle;

        // Named submission for diagnostics. `name` is attached to the job and
        // surfaced by the debugger panel; it has no effect on scheduling.
        auto Schedule(std::string name, std::function<void()> fn, Priority priority = Priority::Normal,
                      std::vector<JobHandle> dependencies = {}) -> JobHandle;

        // Submit independent work or run inline when stopped. During a drain
        // the submission is rejected.
        auto ScheduleOrRun(std::string name, std::function<void()> fn, Priority priority = Priority::Normal)
            -> JobHandle;

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

        // A diagnostic snapshot. The job address is a label only and the
        // copied name remains valid after the worker completes.
        struct RunningJob {
                const Job* job = nullptr;
                std::string name;
        };

        // Snapshot of the jobs currently running, for a debugger panel.
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
        friend class JobHandle;
        enum class State { Stopped, Starting, Running, Draining };

        auto ScheduleImpl(std::string name, std::function<void()> fn, Priority priority,
                          const std::vector<JobHandle>& dependencies, bool run_when_stopped) -> JobHandle;
        auto BeginDrain() -> void;
        auto SignalProgress() -> void;
        auto SubmitRunnable(std::shared_ptr<Job> job) -> void;
        auto TakeRunnable(std::ptrdiff_t worker_index) -> std::shared_ptr<Job>;
        auto WorkerLoop(std::size_t worker_index) -> void;
        auto ExecuteOne(std::shared_ptr<Job> job) -> void;

        const std::uint64_t id_;
        std::mutex lifecycle_mutex_; // Start and external Shutdown
        std::mutex submission_mutex_; // Admission, dependencies and queue table
        std::vector<std::unique_ptr<WorkStealingDeque<std::shared_ptr<Job>>>> queues_;
        std::vector<std::jthread> workers_;
        std::mutex pool_mutex_;
        std::vector<std::shared_ptr<Job>> pool_;
        std::vector<std::shared_ptr<Job>> high_pool_;

        std::atomic<State> state_{State::Stopped};
        std::atomic<std::size_t> worker_count_{0};
        std::atomic<std::uint64_t> progress_{0};
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
