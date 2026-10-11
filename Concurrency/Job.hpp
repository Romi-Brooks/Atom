/**
 * @file           : Job.hpp
 * @brief          : Job handles, priorities, and dependency bookkeeping for the
 *                   Atom job system.
 *
 * A Job is the unit of scheduling: a callable plus a small amount of
 * bookkeeping that lets the scheduler run it once, on some worker, and lets
 * other jobs (or the submitting thread) wait on its completion.
 *
 * Dependency model: explicit continuation counter rather than a general DAG.
 * Every job carries a std::atomic "unfinished" count that starts at one (the
 * job's own body) and is incremented once per dependency. A dependency's
 * completion decrements it; when it reaches one the job is runnable, and when
 * the body runs it reaches zero (complete). This is the same counter-based
 * model used by Naughty Dog and EA's job systems: it avoids the central lock
 * and cache traffic a full dependency graph would introduce.
 */

#ifndef ATOM_CONCURRENCY_JOB_HPP
#define ATOM_CONCURRENCY_JOB_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace atom::job {

// Scheduling priority. Three levels cover every real engine workload; more
// would be over-engineering.
enum class Priority : std::uint8_t {
    Low = 0,    // Background prefetch / speculative work.
    Normal = 1, // Default: decode, metadata, IO, general tasks.
    High = 2,   // Latency-sensitive work that must jump the queue.
};

// A unit of runnable work with dependency bookkeeping. Internal to the
// scheduler; exposed so JobHandle can observe completion without a forward
// declaration cycle.
struct Job {
    std::function<void()> fn;
    Priority priority = Priority::Normal;
    // Optional human-readable label for diagnostics / tooling. Never used for
    // correctness; an empty name means "unnamed". Shown by the JobSystemDebugger
    // so a running job can be identified at a glance.
    std::string name;

    // Number of things that must complete before this job is *complete*:
    //   = 1 (own body) + number of unresolved dependencies.
    // A dependency's completion decrements it; the body's execution decrements
    // it again. 0 == complete.
    std::atomic<std::int32_t> unfinished{1};

    // Jobs that wait on this one. Guarded by cont_mutex: a dependent registers
    // itself here in Schedule(), and the completing thread drains (and clears)
    // this list in ExecuteOne(). The mutex resolves the race where a dependent
    // registers after this job has already completed.
    std::mutex cont_mutex;
    std::vector<std::shared_ptr<Job>> continuations;

    // Set (release) once the body has run and every dependency resolved.
    std::atomic<bool> complete{false};
};

// Snapshot of scheduler throughput, read for diagnostics / tooling. Every
// counter is monotonically increasing except the derived "currently pending".
// All fields are populated from relaxed atomics, so this is an approximate,
// racy-but-safe view intended for a debugger panel, not for correctness logic.
struct JobStats {
    std::uint64_t submitted = 0;   // Total jobs handed to Schedule().
    std::uint64_t completed = 0;   // Total job bodies that finished running.
    std::uint64_t stolen = 0;      // Total successful work-steals (all workers).
    std::uint64_t pending = 0;     // Submitted minus completed (approximate).
};

// Opaque, copyable, reference-counted handle to a submitted job. A handle does
// not own the job; it observes it. Waiting on a handle is safe from any thread.
class JobHandle {
    public:
        JobHandle() = default;

        // True when the handle points at a submitted job (default-constructed
        // handles are empty).
        [[nodiscard]] auto IsValid() const noexcept -> bool;

        // True once the job and every job it depended on have finished.
        [[nodiscard]] auto IsComplete() const noexcept -> bool;

        // Blocks the calling thread until the job completes. The scheduler's
        // WaitFor() is preferred for the main thread because it keeps the
        // caller productive; this is the generic, self-contained form.
        auto Wait() const -> void;

        // True when this handle and `other` refer to the same job.
        [[nodiscard]] auto operator==(const JobHandle& other) const noexcept -> bool;
        [[nodiscard]] auto operator!=(const JobHandle& other) const noexcept -> bool;

    private:
        friend class JobSystem;
        explicit JobHandle(std::shared_ptr<Job> job) : job_(std::move(job)) {}

        std::shared_ptr<Job> job_;
};

} // namespace atom::job

#endif // ATOM_CONCURRENCY_JOB_HPP
