/**
 * @file           : JobSystem.cpp
 * @brief          : Implementation of the work-stealing job scheduler.
 */

#include "JobSystem.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>

#include <Concurrency/ThreadRegistry.hpp>
#include <Log/LogSystem.hpp>

namespace atom::job {

namespace {
std::atomic<std::uint64_t> next_scheduler_id{1};
thread_local std::ptrdiff_t worker_index = -1;
thread_local JobSystem* worker_scheduler = nullptr;

struct Execution;
thread_local Execution* current_job = nullptr;

struct Execution {
    JobSystem* scheduler;
    Execution* previous;

    explicit Execution(JobSystem& owner) : scheduler(&owner), previous(current_job) { current_job = this; }
    ~Execution() { current_job = previous; }
};

auto IsExecuting(const JobSystem* scheduler) -> bool {
    for (auto* job = current_job; job; job = job->previous) {
        if (job->scheduler == scheduler)
            return true;
    }
    return worker_scheduler == scheduler;
}
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
    if (!IsValid() || IsComplete())
        return;
    for (auto* job = current_job; job; job = job->previous) {
        if (job->scheduler->id_ == job_->owner) {
            job->scheduler->WaitFor(*this);
            return;
        }
    }
    job_->complete.wait(false, std::memory_order_acquire);
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

JobSystem::JobSystem() : id_(next_scheduler_id.fetch_add(1, std::memory_order_relaxed)) {
    // Keep the diagnostics registry alive until all scheduler workers exit.
    (void)atom::concurrency::ThreadRegistry::GetInstance();
}

JobSystem::~JobSystem() {
    Shutdown();
}

auto JobSystem::GetInstance() -> JobSystem& {
    static JobSystem instance;
    return instance;
}

auto JobSystem::Configure(const std::size_t count) -> void {
    const std::scoped_lock lock{submission_mutex_};
    if (state_.load() == State::Stopped)
        configured_worker_count_ = count;
}

auto JobSystem::Start() -> bool {
    if (IsExecuting(this))
        return IsRunning();

    const std::scoped_lock lifecycle_lock{lifecycle_mutex_};
    std::unique_lock admission{submission_mutex_};
    const auto state = state_.load();
    if (state != State::Stopped)
        return state == State::Running;

    const auto hardware = std::thread::hardware_concurrency();
    const auto count = configured_worker_count_ != 0 ? configured_worker_count_ : (hardware > 1 ? hardware - 1 : 1);
    queues_.reserve(count);
    workers_.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        queues_.push_back(std::make_unique<WorkStealingDeque<std::shared_ptr<Job>>>(256));

    state_.store(State::Starting);
    try {
        for (std::size_t i = 0; i < count; ++i)
            workers_.emplace_back([this, i] { WorkerLoop(i); });
    } catch (const std::system_error& error) {
        state_.store(State::Draining);
        admission.unlock();
        SignalProgress();
        workers_.clear();
        admission.lock();
        queues_.clear();
        state_.store(State::Stopped);
        admission.unlock();
        LOG_WARNING(atom::log::concurrency::JobSystem, std::string{"Job workers could not start: "} + error.what());
        return false;
    }

    worker_count_.store(count);
    state_.store(State::Running);
    admission.unlock();
    SignalProgress();
    LOG_INFO(atom::log::concurrency::JobSystem, "Job system started with " + std::to_string(count) + " worker(s)");
    return true;
}

auto JobSystem::BeginDrain() -> void {
    {
        const std::scoped_lock admission{submission_mutex_};
        if (state_.load() == State::Stopped && pending_count_.load() == 0)
            return;
        state_.store(State::Draining);
    }
    SignalProgress();
}

auto JobSystem::Shutdown() -> void {
    if (IsExecuting(this)) {
        BeginDrain();
        return;
    }

    const std::scoped_lock lifecycle_lock{lifecycle_mutex_};
    BeginDrain();
    if (state_.load() == State::Stopped)
        return;
    workers_.clear();
    auto pending = pending_count_.load();
    while (pending != 0) {
        pending_count_.wait(pending);
        pending = pending_count_.load();
    }

    {
        const std::scoped_lock admission{submission_mutex_};
        queues_.clear();
        worker_count_.store(0);
        state_.store(State::Stopped);
    }
    LOG_INFO(atom::log::concurrency::JobSystem, "Job system shut down");
}

auto JobSystem::IsRunning() const noexcept -> bool {
    return state_.load() == State::Running;
}

auto JobSystem::WorkerCount() const noexcept -> std::size_t {
    return worker_count_.load();
}

auto JobSystem::Schedule(std::function<void()> fn, const Priority priority,
                         std::vector<JobHandle> dependencies) -> JobHandle {
    return ScheduleImpl({}, std::move(fn), priority, dependencies, false);
}

auto JobSystem::Schedule(std::string name, std::function<void()> fn, const Priority priority,
                         std::vector<JobHandle> dependencies) -> JobHandle {
    return ScheduleImpl(std::move(name), std::move(fn), priority, dependencies, false);
}

auto JobSystem::Schedule(std::function<void()> fn, const JobHandle dependency, const Priority priority)
    -> JobHandle {
    return ScheduleImpl({}, std::move(fn), priority, {dependency}, false);
}

auto JobSystem::ScheduleOrRun(std::string name, std::function<void()> fn, const Priority priority) -> JobHandle {
    return ScheduleImpl(std::move(name), std::move(fn), priority, {}, true);
}

auto JobSystem::ScheduleImpl(std::string name, std::function<void()> fn, const Priority priority,
                             const std::vector<JobHandle>& dependencies, const bool run_when_stopped) -> JobHandle {
    std::unique_lock admission{submission_mutex_};
    const auto state = state_.load();
    const bool inline_job = run_when_stopped && state == State::Stopped;
    if (state != State::Running && !inline_job)
        return {};
    for (const auto& dependency : dependencies) {
        if (dependency.IsValid() && dependency.job_->owner != id_)
            return {};
    }

    auto job = std::make_shared<Job>();
    job->owner = id_;
    job->fn = std::move(fn);
    job->name = std::move(name);
    job->priority = priority;
    for (const auto& dependency : dependencies) {
        if (dependency.IsValid() && !dependency.IsComplete()) {
            dependency.job_->continuations.push_back(job);
            ++job->remaining_dependencies;
        }
    }
    pending_count_.fetch_add(1);
    submitted_count_.fetch_add(1, std::memory_order_relaxed);
    if (inline_job) {
        admission.unlock();
        ExecuteOne(job);
    } else if (job->remaining_dependencies == 0) {
        SubmitRunnable(job);
    }
    return JobHandle{std::move(job)};
}

auto JobSystem::WaitFor(const JobHandle& handle) -> void {
    if (!handle.IsValid() || handle.IsComplete())
        return;
    if (handle.job_->owner != id_) {
        handle.Wait();
        return;
    }
    while (!handle.IsComplete()) {
        const auto progress = progress_.load();
        std::shared_ptr<Job> job;
        {
            const std::scoped_lock admission{submission_mutex_};
            job = TakeRunnable(worker_scheduler == this ? worker_index : -1);
        }
        if (job) {
            ExecuteOne(std::move(job));
        } else if (!handle.IsComplete()) {
            progress_.wait(progress);
        }
    }
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
    stats.pending = pending_count_.load(std::memory_order_relaxed);
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

auto JobSystem::SignalProgress() -> void {
    progress_.fetch_add(1);
    progress_.notify_all();
}

auto JobSystem::SubmitRunnable(std::shared_ptr<Job> job) -> void {
    if (job->priority != Priority::High && worker_scheduler == this && queues_[worker_index]->Push(job)) {
        SignalProgress();
        return;
    }
    {
        const std::scoped_lock lock{pool_mutex_};
        auto& pool = job->priority == Priority::High ? high_pool_ : pool_;
        pool.push_back(std::move(job));
    }
    SignalProgress();
}

auto JobSystem::TakeRunnable(const std::ptrdiff_t index) -> std::shared_ptr<Job> {
    if (index >= 0) {
        if (auto own = queues_[index]->Pop())
            return std::move(*own);
    }
    {
        const std::scoped_lock lock{pool_mutex_};
        auto& pool = high_pool_.empty() ? pool_ : high_pool_;
        if (!pool.empty()) {
            auto job = std::move(pool.back());
            pool.pop_back();
            return job;
        }
    }
    for (std::size_t i = 0; i < queues_.size(); ++i) {
        const auto victim = (static_cast<std::size_t>(index + 1) + i) % queues_.size();
        if (static_cast<std::ptrdiff_t>(victim) == index)
            continue;
        if (auto stolen = queues_[victim]->Steal()) {
            stolen_count_.fetch_add(1, std::memory_order_relaxed);
            return std::move(*stolen);
        }
    }
    return {};
}

auto JobSystem::WorkerLoop(const std::size_t index) -> void {
    worker_scheduler = this;
    worker_index = static_cast<std::ptrdiff_t>(index);
    atom::concurrency::ThreadRegistry::ScopedRegistration registration{"JobWorker-" + std::to_string(index)};
    while (true) {
        const auto progress = progress_.load();
        if (state_.load() == State::Draining && pending_count_.load() == 0)
            break;
        if (auto job = TakeRunnable(worker_index))
            ExecuteOne(std::move(job));
        else
            progress_.wait(progress);
    }
    worker_index = -1;
    worker_scheduler = nullptr;
}

auto JobSystem::ExecuteOne(std::shared_ptr<Job> job) -> void {
    const Execution execution{*this};
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

    job->fn = {};
    {
        const std::scoped_lock admission{submission_mutex_};
        completed_count_.fetch_add(1, std::memory_order_relaxed);
        pending_count_.fetch_sub(1);
        job->complete.store(true, std::memory_order_release);
        for (auto& dependent : job->continuations) {
            if (--dependent->remaining_dependencies == 0)
                SubmitRunnable(std::move(dependent));
        }
        job->continuations.clear();
        job->complete.notify_all();
        pending_count_.notify_all();
        SignalProgress();
    }
}

} // namespace atom::job
