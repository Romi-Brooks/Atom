/**
 * @file           : Job.hpp
 * @brief          : Job handles, priorities, and dependency bookkeeping for the
 *                   Atom job system.
 *
 * A Job is the unit of scheduling: a callable plus a small amount of
 * bookkeeping that lets the scheduler run it once, on some worker, and lets
 * other jobs (or the submitting thread) wait on its completion.
 *
 */

#ifndef ATOM_CONCURRENCY_JOB_HPP
#define ATOM_CONCURRENCY_JOB_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace atom::job {

// Scheduling priority. Three levels cover every real engine workload; more
// would be over-engineering.
enum class Priority : std::uint8_t {
    Low = 0,    // Background prefetch / speculative work.
    Normal = 1, // Default: decode, metadata, IO, general tasks.
    High = 2,   // Latency-sensitive work that must jump the queue.
};

class JobSystem;

struct Job {
    std::uint64_t owner = 0;
    std::function<void()> fn;
    Priority priority = Priority::Normal;
    std::string name;
    // The scheduler protects dependency registration and release.
    std::size_t remaining_dependencies = 0;
    std::vector<std::shared_ptr<Job>> continuations;
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
    std::uint64_t pending = 0;     // Accepted work that has yet to finish.
};

// Copyable handle to a submitted job. The handle can outlive the scheduler.
class JobHandle {
    public:
        JobHandle() = default;

        // True when the handle points at a submitted job (default-constructed
        // handles are empty).
        [[nodiscard]] auto IsValid() const noexcept -> bool;

        // True once the job and every job it depended on have finished.
        [[nodiscard]] auto IsComplete() const noexcept -> bool;

        // Waits for completion. Jobs waiting on their own scheduler can help it.
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
