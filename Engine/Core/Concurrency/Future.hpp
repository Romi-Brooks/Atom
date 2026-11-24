/**
 * @file           : Future.hpp
 * @brief          : One-shot, thread-safe future/promise for asynchronous loads.
 *
 * ARCH-107 (async resource loading) primitive. A Promise is the single producer
 * (usually a job-system worker); a Future is the consumer side polled by the
 * render thread. The value handoff is a one-shot, mutex-guarded box: the
 * producer transitions Pending -> Ready/Failed/Cancelled exactly once and
 * notifies any waiter.
 *
 * Design (frozen contract):
 *   - Pull model: the render thread polls IsReady()/State() each frame and reads
 *     the result; there is no completion callback pushed onto the render thread.
 *   - Generation: a caller-assigned tag carried for correlation and diagnostics;
 *     invalidation is expressed by Cancel() (a cancelled future never becomes
 *     Ready).
 *   - Progress: a coarse 0..1 float + message for UI, only meaningful while
 *     Pending.
 *   - T must be cheaply copyable (a handle or shared_ptr); Get() returns by value.
 *
 * The asset layer builds ResourceManager::AcquireAsync on top of this; a worker
 * loads the raw resource and the render thread finalizes it into a handle (see
 * Docs/Subsystem-Plans-CN.md).
**/

#ifndef ATOM_CONCURRENCY_FUTURE_HPP
#define ATOM_CONCURRENCY_FUTURE_HPP

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <Error/Error.hpp>

namespace atom::async {

// Lifecycle of a Future. Pending is the only non-terminal state.
enum class FutureState : std::uint8_t {
    Pending = 0,
    Ready,
    Failed,
    Cancelled,
};

namespace detail {

template <typename T> struct FutureSharedState {
        std::mutex mutex;
        std::condition_variable cv;
        FutureState state = FutureState::Pending;
        float progress = 0.0f;
        std::string progress_message{};
        std::uint64_t generation = 0;
        std::optional<Result<T>> result{}; // set once when Ready or Failed

        FutureSharedState() = default;
        FutureSharedState(const FutureSharedState&) = delete;
        FutureSharedState& operator=(const FutureSharedState&) = delete;
};

} // namespace detail

template <typename T> class Promise;

// Copyable consumer handle (consistent with ResourceHandle / JobHandle). The
// shared state stays alive until the last Future or Promise referencing it is
// dropped.
template <typename T> class Future {
    public:
        Future() = default;

        // True once the operation is terminal (Ready / Failed / Cancelled).
        [[nodiscard]] auto IsReady() const -> bool {
            return State() != FutureState::Pending;
        }

        [[nodiscard]] auto State() const -> FutureState {
            if (!state_)
                return FutureState::Pending;
            std::lock_guard lock{state_->mutex};
            return state_->state;
        }

        // Coarse 0..1 progress; meaningful only while Pending.
        [[nodiscard]] auto Progress() const -> float {
            if (!state_)
                return 0.0f;
            std::lock_guard lock{state_->mutex};
            return state_->progress;
        }

        // Human-readable stage label. The view is valid until the next
        // SetProgress() or completion, so read it within the same frame.
        [[nodiscard]] auto ProgressMessage() const -> std::string_view {
            if (!state_)
                return {};
            std::lock_guard lock{state_->mutex};
            return state_->progress_message;
        }

        [[nodiscard]] auto Generation() const -> std::uint64_t {
            if (!state_)
                return 0;
            std::lock_guard lock{state_->mutex};
            return state_->generation;
        }

        // Non-blocking result access. Returns nullptr while Pending; otherwise a
        // pointer into the shared state, valid as long as this Future is alive.
        [[nodiscard]] auto TryGet() const -> const Result<T>* {
            if (!state_)
                return nullptr;
            std::lock_guard lock{state_->mutex};
            return state_->result ? &*state_->result : nullptr;
        }

        // Blocks until terminal, then returns the result (a copy). Cancelled
        // yields a FutureCancelled error; an empty future yields InvalidArgument.
        // Prefer polling IsReady() on the render thread rather than blocking.
        [[nodiscard]] auto Get() -> Result<T> {
            if (!state_)
                return std::unexpected<Error>{MakeError(ErrorCode::InvalidArgument, Module::Concurrency, "Future::Get")};
            std::unique_lock lock{state_->mutex};
            state_->cv.wait(lock, [&] { return state_->state != FutureState::Pending; });
            if (state_->state == FutureState::Cancelled)
                return std::unexpected<Error>{
                    MakeError(ErrorCode::FutureCancelled, Module::Concurrency, "Future::Get")};
            return *state_->result;
        }

        // Marks a Pending future as cancelled; a no-op once terminal.
        auto Cancel() -> void {
            if (!state_)
                return;
            {
                std::lock_guard lock{state_->mutex};
                if (state_->state != FutureState::Pending)
                    return;
                state_->state = FutureState::Cancelled;
            }
            state_->cv.notify_all();
        }

        [[nodiscard]] explicit operator bool() const {
            return state_ != nullptr;
        }

    private:
        friend class Promise<T>;
        explicit Future(std::shared_ptr<detail::FutureSharedState<T>> state) : state_(std::move(state)) {}
        std::shared_ptr<detail::FutureSharedState<T>> state_{};
};

// Single producer. Usually owned by the loader glue (worker side); the Future
// handed to the caller observes the same shared state.
template <typename T> class Promise {
    public:
        explicit Promise(std::uint64_t generation = 0)
            : state_(std::make_shared<detail::FutureSharedState<T>>()) {
            state_->generation = generation;
        }

        Promise(const Promise&) = delete;
        auto operator=(const Promise&) = delete;
        Promise(Promise&&) noexcept = default;
        auto operator=(Promise&&) noexcept -> Promise& = default;

        [[nodiscard]] auto GetFuture() const -> Future<T> {
            return Future<T>{state_};
        }

        [[nodiscard]] auto Generation() const -> std::uint64_t {
            std::lock_guard lock{state_->mutex};
            return state_->generation;
        }

        auto SetProgress(float progress, std::string_view message = {}) -> void {
            if (!state_)
                return;
            std::lock_guard lock{state_->mutex};
            if (state_->state != FutureState::Pending)
                return;
            state_->progress = std::clamp(progress, 0.0f, 1.0f);
            state_->progress_message = std::string{message};
        }

        // Pending -> Ready. A no-op once terminal (one-shot).
        auto SetValue(T value) -> void {
            if (!state_)
                return;
            {
                std::lock_guard lock{state_->mutex};
                if (state_->state != FutureState::Pending)
                    return;
                state_->result = Result<T>{std::move(value)};
                state_->state = FutureState::Ready;
            }
            state_->cv.notify_all();
        }

        // Pending -> Failed. A no-op once terminal (one-shot).
        auto SetError(Error error) -> void {
            if (!state_)
                return;
            {
                std::lock_guard lock{state_->mutex};
                if (state_->state != FutureState::Pending)
                    return;
                state_->result = Result<T>{std::unexpected<Error>{std::move(error)}};
                state_->state = FutureState::Failed;
            }
            state_->cv.notify_all();
        }

        auto Cancel() -> void {
            GetFuture().Cancel();
        }

        [[nodiscard]] explicit operator bool() const {
            return state_ != nullptr;
        }

    private:
        std::shared_ptr<detail::FutureSharedState<T>> state_{};
};

} // namespace atom::async

#endif // ATOM_CONCURRENCY_FUTURE_HPP
