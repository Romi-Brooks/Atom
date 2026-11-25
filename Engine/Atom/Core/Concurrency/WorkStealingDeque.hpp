#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace atom::job {

// Bounded worker queue. The owner pops newest jobs; thieves take the oldest.
template <typename T>
class WorkStealingDeque {
    public:
        explicit WorkStealingDeque(std::size_t capacity) : buffer_(capacity) {}

        auto Push(T item) -> bool {
            const std::scoped_lock lock{mutex_};
            if (size_ == buffer_.size())
                return false;
            buffer_[(front_ + size_) % buffer_.size()].emplace(std::move(item));
            ++size_;
            return true;
        }

        auto Pop() -> std::optional<T> {
            const std::scoped_lock lock{mutex_};
            if (size_ == 0)
                return std::nullopt;
            --size_;
            return std::exchange(buffer_[(front_ + size_) % buffer_.size()], std::nullopt);
        }

        auto Steal() -> std::optional<T> {
            const std::scoped_lock lock{mutex_};
            if (size_ == 0)
                return std::nullopt;
            auto item = std::exchange(buffer_[front_], std::nullopt);
            front_ = (front_ + 1) % buffer_.size();
            --size_;
            return item;
        }

        [[nodiscard]] auto ApproximateSize() const -> std::size_t {
            const std::scoped_lock lock{mutex_};
            return size_;
        }

    private:
        mutable std::mutex mutex_;
        std::vector<std::optional<T>> buffer_;
        std::size_t front_ = 0;
        std::size_t size_ = 0;
};

} // namespace atom::job
