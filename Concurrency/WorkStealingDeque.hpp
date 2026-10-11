/**
 * @file           : WorkStealingDeque.hpp
 * @brief          : Chase-Lev bounded work-stealing deque.
 *
 * Each worker owns a deque of pending jobs. The owner pushes and pops from the
 * back (LIFO, lock-free, cheap), while other workers steal from the front
 * (FIFO, lock-free via a single compare-exchange on the front index). This is
 * the classic Chase-Lev design (Nimar Arora, Robert Blumofe, Greg Plaxton) and
 * the same structure behind enkiTS, EA's job system and rayon.
 *
 * Memory ordering is the subtle part. The rules encoded here:
 *   - push    : relaxed bump of bottom, release-store the item, then
 *               release-increment of the actual bottom.
 *   - pop     : load bottom (acquire), then relaxed load of top, and a single
 *               relaxed CAS on top is the linearization point for the final
 *               item.
 *   - steal   : relaxed load of top, acquire-load of bottom, then CAS on top
 *               (acq_rel) to claim the item; a failed CAS re-reads.
 *
 * The deque is fixed-capacity and single-producer (the owner) / multi-consumer
 * (all stealers). Bounded means a full deque silently drops pushes rather than
 * allocating or blocking — jobs are always re-submittable, so dropping is a
 * safe back-pressure strategy for a game loop.
 */

#ifndef ATOM_CONCURRENCY_WORK_STEALING_DEQUE_HPP
#define ATOM_CONCURRENCY_WORK_STEALING_DEQUE_HPP

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace atom::job {

// T must be cheaply copyable; the queue stores by value.
template <typename T>
class WorkStealingDeque {
    public:
        explicit WorkStealingDeque(std::size_t capacity) : capacity_(capacity), buffer_(capacity) {
            // Capacity must be a power of two so index arithmetic uses masks.
            assert((capacity & (capacity - 1)) == 0 && "capacity must be a power of two");
            mask_ = capacity - 1;
        }

        WorkStealingDeque(const WorkStealingDeque&) = delete;
        auto operator=(const WorkStealingDeque&) -> WorkStealingDeque& = delete;

        // Owner-only: push an item onto the back. Returns false if full.
        auto Push(T item) -> bool {
            const auto b = bottom_.load(std::memory_order_relaxed);
            const auto t = top_.load(std::memory_order_acquire);
            if (b - t >= static_cast<std::int64_t>(capacity_)) {
                return false; // full
            }
            buffer_[b & mask_] = std::move(item);
            // The item must be visible before bottom advances past it.
            std::atomic_thread_fence(std::memory_order_release);
            bottom_.store(b + 1, std::memory_order_release);
            return true;
        }

        // Owner-only: pop the most recent item (LIFO). Empty if nothing.
        auto Pop() -> std::optional<T> {
            auto b = bottom_.load(std::memory_order_relaxed);
            if (b == top_.load(std::memory_order_acquire)) {
                return std::nullopt; // empty
            }
            b -= 1;
            bottom_.store(b, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            auto t = top_.load(std::memory_order_relaxed);
            if (t <= b) {
                auto item = std::move(buffer_[b & mask_]);
                if (t == b) {
                    // Last element: race with a stealer for it.
                    if (!top_.compare_exchange_strong(
                            t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                        // Lost the race; a stealer took it.
                        bottom_.store(b + 1, std::memory_order_relaxed);
                        return std::nullopt;
                    }
                    bottom_.store(b + 1, std::memory_order_relaxed);
                }
                return item;
            }
            // Empty: restore bottom and bail.
            bottom_.store(b + 1, std::memory_order_relaxed);
            return std::nullopt;
        }

        // Any thread: steal the oldest item (FIFO). Empty if nothing.
        auto Steal() -> std::optional<T> {
            auto t = top_.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            const auto b = bottom_.load(std::memory_order_acquire);
            if (t >= b) {
                return std::nullopt; // empty
            }
            auto item = buffer_[t & mask_];
            // Single CAS is the linearization point for the steal.
            if (!top_.compare_exchange_strong(t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return std::nullopt; // lost to another stealer
            }
            return item;
        }

        // Approximate number of items; useful for diagnostics only.
        [[nodiscard]] auto ApproximateSize() const noexcept -> std::size_t {
            const auto t = top_.load(std::memory_order_relaxed);
            const auto b = bottom_.load(std::memory_order_relaxed);
            return static_cast<std::size_t>(b >= t ? b - t : 0);
        }

    private:
        std::size_t capacity_;
        std::size_t mask_;
        std::vector<T> buffer_;          // pre-sized; indices masked to capacity
        std::atomic<std::int64_t> top_{0};
        std::atomic<std::int64_t> bottom_{0};
};

} // namespace atom::job

#endif // ATOM_CONCURRENCY_WORK_STEALING_DEQUE_HPP
