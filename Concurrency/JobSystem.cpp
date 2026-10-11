/**
 * @file           : JobSystem.cpp
 * @brief          : Implementation of the work-stealing job scheduler.
 */

#include "JobSystem.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

#include <Log/LogSystem.hpp>

namespace atom::job {

namespace {
// The worker index of the calling thread, or -1 for non-worker threads (main
// thread, helpers, external submitters). Written once at worker startup; read
// on every SubmitRunnable() to route jobs to the local lock-free queue instead
// of the shared pool.
thread_local std::ptrdiff_t g_worker_index = -1;
} // namespace

// ---------------------------------------------------------------------------
// JobHandle
// ---------------------------------------------------------------------------

auto JobHandle::IsValid() const noexcept -> bool {
    return job_ != nullptr;
}

auto JobHandle::IsComplete() const noexcept -> bool {
    return job_ && job_->complete.load(std::memory_order_acquire);
}

auto JobHandle::Wait() const -> void {
    if (!IsValid())
        return;
    JobSystem::GetInstance().WaitFor(*this);
}

auto JobHandle::operator==(const JobHandle& other) const noexcept -> bool {
    return job_ == other.job_;
}
auto JobHandle::operator!=(const JobHandle& other) const noexcept -> bool {
    return !(*this == other);
}

// ---------------------------------------------------------------------------
// JobSystem
// ---------------------------------------------------------------------------

JobSystem::JobSystem() = default;

JobSystem::~JobSystem() {
    Shutdown();
}

auto JobSystem::GetInstance() -> JobSystem& {
    static JobSystem instance;
    return instance;
}

auto JobSystem::Configure(const std::size_t worker_count) -> void {
    if (running_.load(std::memory_order_acquire))
        return;
    if (worker_count == 0) {
        const auto hw = std::thread::hardware_concurrency();
        configured_worker_count_ = hw > 1 ? hw - 1 : 1;
    } else {
        configured_worker_count_ = worker_count;
    }
}

auto JobSystem::Start() -> bool {
    if (running_.load(std::memory_order_acquire))
        return true;
    if (configured_worker_count_ == 0)
        Configure();

    // A power-of-two capacity per queue keeps index math cheap.
    constexpr std::size_t queue_capacity = 256;
    queues_.clear();
    workers_.clear();
    queues_.reserve(configured_worker_count_);
    workers_.reserve(configured_worker_count_);

    running_.store(true, std::memory_order_release);

    for (std::size_t i = 0; i < configured_worker_count_; ++i) {
        auto queue = std::make_unique<WorkStealingDeque<std::shared_ptr<Job>>>(queue_capacity);
        auto worker = std::make_unique<Worker>();
        worker->queue = queue.get();
        worker->thread = std::jthread([this, i](std::stop_token stop) { WorkerLoop(i, stop); });
        queues_.push_back(std::move(queue));
        workers_.push_back(std::move(worker));
    }

    LOG_INFO(atom::log::concurrency::JobSystem,
             "Job system started with " + std::to_string(workers_.size()) + " worker(s)");
    return !workers_.empty();
}

auto JobSystem::Shutdown() -> void {
    if (!running_.exchange(false, std::memory_order_acq_rel))
        return;

    // Wake every worker so it observes the shutdown flag and exits.
    pool_cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker->thread.joinable())
            worker->thread.request_stop();
    }
    for (auto& worker : workers_) {
        if (worker->thread.joinable())
            worker->thread.join();
    }
    workers_.clear();
    queues_.clear();

    // Drop any stranded jobs. Handles remain valid (their completion is
    // observed independently), but jobs that never ran are marked complete so
    // waiters do not block forever.
    {
        std::scoped_lock lock(pool_mutex_);
        for (auto& job : pool_) {
            job->complete.store(true, std::memory_order_release);
            job->unfinished.store(0, std::memory_order_release);
        }
        for (auto& job : high_pool_) {
            job->complete.store(true, std::memory_order_release);
            job->unfinished.store(0, std::memory_order_release);
        }
        pool_.clear();
        high_pool_.clear();
    }
    pending_count_.store(0, std::memory_order_release);
    pool_cv_.notify_all();
    LOG_INFO(atom::log::concurrency::JobSystem, "Job system shut down");
}

auto JobSystem::IsRunning() const noexcept -> bool {
    return running_.load(std::memory_order_acquire);
}

auto JobSystem::WorkerCount() const noexcept -> std::size_t {
    return workers_.size();
}

auto JobSystem::Schedule(std::function<void()> fn, const Priority priority,
                         std::vector<JobHandle> dependencies) -> JobHandle {
    return Schedule(std::string{}, std::move(fn), priority, std::move(dependencies));
}

auto JobSystem::Schedule(std::string name, std::function<void()> fn, const Priority priority,
                         std::vector<JobHandle> dependencies) -> JobHandle {
    auto job = std::make_shared<Job>();
    job->fn = std::move(fn);
    job->priority = priority;
    job->name = std::move(name);

    // "unfinished" counts the job's own body (1) plus every unresolved
    // dependency. We resolve "already-complete" dependencies immediately below.
    std::vector<std::shared_ptr<Job>> valid_deps;
    valid_deps.reserve(dependencies.size());
    for (auto& dep : dependencies) {
        if (dep.IsValid())
            valid_deps.push_back(dep.job_);
        // Invalid/empty handles are trivially satisfied and not counted.
    }
    job->unfinished.store(1 + static_cast<std::int32_t>(valid_deps.size()), std::memory_order_release);

    pending_count_.fetch_add(1, std::memory_order_relaxed);
    submitted_count_.fetch_add(1, std::memory_order_relaxed);

    // Register this job as a continuation of each dependency. The dependency
    // may have *already* completed between our read of `IsValid` and now, so
    // registration is done under the dependency's mutex and re-checks its
    // completion flag: a completed dependency counts as satisfied immediately
    // instead of waiting on a continuation that will never be drained.
    for (auto& dep_job : valid_deps) {
        bool already_done = false;
        {
            std::scoped_lock lock(dep_job->cont_mutex);
            if (dep_job->complete.load(std::memory_order_acquire)) {
                already_done = true;
            } else {
                dep_job->continuations.push_back(job);
            }
        }
        if (already_done) {
            job->unfinished.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    // If the job is now runnable (its own body is all that remains), submit it.
    // This is the single decision point: either it was scheduled with no
    // unresolved dependencies, or every dependency was already complete.
    if (job->unfinished.load(std::memory_order_acquire) == 1) {
        SubmitRunnable(job);
    }

    return JobHandle{std::move(job)};
}

auto JobSystem::Schedule(std::function<void()> fn, const JobHandle dependency, const Priority priority)
    -> JobHandle {
    std::vector<JobHandle> deps;
    if (dependency.IsValid())
        deps.push_back(dependency);
    return Schedule(std::move(fn), priority, std::move(deps));
}

auto JobSystem::WaitFor(const JobHandle& handle) -> void {
    if (!handle.IsValid() || handle.IsComplete())
        return;
    // If the system is not running, there is no worker to complete the job;
    // avoid an unbounded spin. A job submitted before Shutdown but never run
    // was marked complete by Shutdown(), so this is only reached when the job
    // is genuinely unfulfillable — yield rather than hang.
    if (!running_.load(std::memory_order_acquire)) {
        while (!handle.IsComplete())
            std::this_thread::yield();
        return;
    }
    const auto job = handle.job_;
    HelpWhileWaiting([&job] { return job->complete.load(std::memory_order_acquire); });
}

auto JobSystem::WaitAll(const std::vector<JobHandle>& handles) -> void {
    for (const auto& h : handles)
        WaitFor(h);
}

auto JobSystem::PendingJobCount() const noexcept -> std::size_t {
    return pending_count_.load(std::memory_order_relaxed);
}

auto JobSystem::Stats() const noexcept -> JobStats {
    JobStats stats;
    stats.submitted = submitted_count_.load(std::memory_order_relaxed);
    stats.completed = completed_count_.load(std::memory_order_relaxed);
    stats.stolen = stolen_count_.load(std::memory_order_relaxed);
    // pending is derived; clamp to avoid a transient underflow when a read of
    // `completed` races ahead of `submitted` across the two relaxed loads.
    stats.pending = stats.submitted >= stats.completed ? stats.submitted - stats.completed : 0;
    return stats;
}

auto JobSystem::GetRunningJobs() const -> std::vector<RunningJob> {
    std::scoped_lock lock(running_mutex_);
    std::vector<RunningJob> snapshot;
    snapshot.reserve(running_jobs_.size());
    for (const auto* job : running_jobs_)
        snapshot.push_back(RunningJob{job, job->name});
    return snapshot;
}

auto JobSystem::GetRecentJobs() const -> std::vector<FinishedJob> {
    std::scoped_lock lock(running_mutex_);
    return {recent_jobs_.begin(), recent_jobs_.end()};
}

// ---------------------------------------------------------------------------
// Scheduling internals
// ---------------------------------------------------------------------------

auto JobSystem::CurrentWorkerIndex() const noexcept -> std::ptrdiff_t {
    return g_worker_index;
}

auto JobSystem::SubmitRunnable(std::shared_ptr<Job> job) -> void {
    // High-priority work always goes to the shared pool so it jumps the queue
    // ahead of any local backlog. Otherwise, a worker submitting from its own
    // thread uses its lock-free local queue (LIFO); every other thread uses the
    // shared pool. A full local queue falls back to the pool so a job is never
    // silently dropped.
    const auto worker_idx = g_worker_index;
    if (job->priority != Priority::High && worker_idx >= 0 &&
        static_cast<std::size_t>(worker_idx) < queues_.size() && queues_[worker_idx]) {
        if (queues_[worker_idx]->Push(job)) {
            return;
        }
        // Fall through to the pool on overflow.
    }
    PushToPool(std::move(job));
}

auto JobSystem::PushToPool(std::shared_ptr<Job> job) -> void {
    {
        std::scoped_lock lock(pool_mutex_);
        if (job->priority == Priority::High)
            high_pool_.push_back(std::move(job));
        else
            pool_.push_back(std::move(job));
    }
    pool_cv_.notify_one();
}

auto JobSystem::TryPopPool(std::shared_ptr<Job>& out) -> bool {
    std::scoped_lock lock(pool_mutex_);
    if (!high_pool_.empty()) {
        out = std::move(high_pool_.back());
        high_pool_.pop_back();
        return true;
    }
    if (!pool_.empty()) {
        out = std::move(pool_.back());
        pool_.pop_back();
        return true;
    }
    return false;
}

auto JobSystem::WorkerLoop(const std::size_t worker_index, std::stop_token stop) -> void {
    g_worker_index = static_cast<std::ptrdiff_t>(worker_index);
    while (!stop.stop_requested() && running_.load(std::memory_order_acquire)) {
        if (RunOne(worker_index)) {
            continue; // ran a job; try again immediately (drains local LIFO)
        }
        // Nothing to run: block until work arrives or shutdown.
        std::unique_lock lock(pool_mutex_);
        pool_cv_.wait_for(lock, std::chrono::milliseconds(1), [this, stop] {
            return !running_.load(std::memory_order_acquire) || !pool_.empty() || !high_pool_.empty();
        });
    }
    g_worker_index = -1;
}

auto JobSystem::RunOne(const std::size_t worker_index) -> bool {
    std::shared_ptr<Job> job;

    // 1. Own queue (LIFO, cheapest, lock-free).
    if (worker_index < queues_.size() && queues_[worker_index]) {
        if (auto item = queues_[worker_index]->Pop()) {
            job = std::move(*item);
        }
    }

    // 2. Shared pool (cross-thread submissions and high-priority work).
    if (!job)
        (void)TryPopPool(job);

    // 3. Steal from another worker (FIFO) to balance load.
    if (!job && !queues_.empty()) {
        const auto n = queues_.size();
        const auto start = (worker_index + 1) % n;
        for (std::size_t k = 0; k < n && !job; ++k) {
            const auto idx = (start + k) % n;
            if (idx == worker_index)
                continue;
            if (auto item = queues_[idx]->Steal()) {
                job = std::move(*item);
                stolen_count_.fetch_add(1, std::memory_order_relaxed);
                break;
            }
        }
    }

    if (!job)
        return false;

    ExecuteOne(std::move(job));
    return true;
}

auto JobSystem::ExecuteOne(std::shared_ptr<Job> job) -> bool {
    // Register this job as running so the debugger panel can list it. The raw
    // pointer stays valid for the whole body: the shared_ptr is alive on this
    // stack and the scheduler does not free jobs while they run.
    {
        std::scoped_lock lock(running_mutex_);
        running_jobs_.push_back(job.get());
    }

    // Most jobs complete far faster than a frame, so a "running" snapshot is
    // nearly always empty; also record the duration into a bounded ring of
    // recent completions so the panel has something meaningful to display.
    const auto body_start = std::chrono::steady_clock::now();

    // Run the job body. A throwing body must not kill the worker thread.
    try {
        job->fn();
    } catch (const std::exception& e) {
        // Swallow: the job system never propagates exceptions across threads.
        // Log once (WARNING, low-frequency) so a throwing job does not vanish
        // silently — this is a diagnostics signal, not a per-job trace.
        LOG_WARNING(atom::log::concurrency::JobSystem,
                    std::string{"Job threw and was swallowed: "} + (job->name.empty() ? "<unnamed>" : job->name) +
                        " (" + e.what() + ")");
    } catch (...) {
        LOG_WARNING(atom::log::concurrency::JobSystem,
                    std::string{"Job threw a non-std exception and was swallowed: "} +
                        (job->name.empty() ? "<unnamed>" : job->name));
    }

    {
        std::scoped_lock lock(running_mutex_);
        const auto it = std::find(running_jobs_.begin(), running_jobs_.end(), job.get());
        if (it != running_jobs_.end())
            running_jobs_.erase(it);

        const auto body_end = std::chrono::steady_clock::now();
        FinishedJob record;
        record.name = job->name; // Copy: the Job dies with our shared_ptr.
        record.duration_ms =
            std::chrono::duration<double, std::milli>(body_end - body_start).count();
        record.finished_at =
            std::chrono::duration<double>(body_end.time_since_epoch()).count();
        recent_jobs_.push_back(std::move(record));
        while (recent_jobs_.size() > recent_jobs_capacity_)
            recent_jobs_.pop_front();
    }

    // The job body is done: drop the "self" contribution from unfinished.
    const auto remaining = job->unfinished.fetch_sub(1, std::memory_order_acq_rel) - 1;
    pending_count_.fetch_sub(1, std::memory_order_relaxed);
    completed_count_.fetch_add(1, std::memory_order_relaxed);

    if (remaining == 0) {
        // Fully complete: no unresolved dependencies remain.
        job->complete.store(true, std::memory_order_release);
    }

    // Drain and signal every dependent. The mutex pairs with Schedule()'s
    // registration lock so a dependent that registers after we drain is either
    // (a) seen here and decremented now, or (b) detects our `complete` flag and
    // self-decrements. A dependent whose count reaches one has all dependencies
    // satisfied and becomes runnable (its own body, the final "1", remains).
    std::vector<std::shared_ptr<Job>> dependents;
    {
        std::scoped_lock lock(job->cont_mutex);
        dependents.swap(job->continuations);
    }
    for (auto& dep : dependents) {
        const auto dep_remaining = dep->unfinished.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (dep_remaining == 1) {
            SubmitRunnable(dep);
        }
    }
    return true;
}

auto JobSystem::HelpWhileWaiting(const std::function<bool()>& done) -> void {
    // Run pending jobs from the pool until the condition is met. A helper has
    // no local queue, so it drains the shared pool and steals from workers.
    while (!done()) {
        std::shared_ptr<Job> job;
        if (TryPopPool(job)) {
            ExecuteOne(std::move(job));
            continue;
        }
        // Nothing in the pool: try stealing from a worker's queue.
        bool stole = false;
        for (auto& q : queues_) {
            if (q) {
                if (auto item = q->Steal()) {
                    ExecuteOne(std::move(*item));
                    stolen_count_.fetch_add(1, std::memory_order_relaxed);
                    stole = true;
                    break;
                }
            }
        }
        if (stole)
            continue;
        // Nothing runnable anywhere: yield and re-check.
        if (done())
            break;
        std::this_thread::yield();
    }
}

} // namespace atom::job
