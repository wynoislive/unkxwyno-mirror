#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace unkxwyno::core {

enum class QueueOverflowPolicy {
    DropOldest,
    RejectNewest,
};

struct QueuePushResult {
    bool accepted{};
    bool dropped_item{};
};

template <typename T>
class BoundedQueue final {
public:
    explicit BoundedQueue(
        std::size_t capacity,
        QueueOverflowPolicy overflow_policy = QueueOverflowPolicy::DropOldest)
        : capacity_(capacity), overflow_policy_(overflow_policy) {
        if (capacity_ == 0) {
            throw std::invalid_argument("BoundedQueue capacity must be greater than zero");
        }
    }

    [[nodiscard]] QueuePushResult push(T value) {
        std::scoped_lock lock(mutex_);
        if (items_.size() == capacity_) {
            if (overflow_policy_ == QueueOverflowPolicy::RejectNewest) {
                return {.accepted = false, .dropped_item = true};
            }
            items_.pop_front();
            items_.push_back(std::move(value));
            return {.accepted = true, .dropped_item = true};
        }
        items_.push_back(std::move(value));
        return {.accepted = true, .dropped_item = false};
    }

    [[nodiscard]] std::optional<T> try_pop() {
        std::scoped_lock lock(mutex_);
        if (items_.empty()) {
            return std::nullopt;
        }
        T value = std::move(items_.front());
        items_.pop_front();
        return value;
    }

    [[nodiscard]] std::size_t size() const {
        std::scoped_lock lock(mutex_);
        return items_.size();
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return capacity_;
    }

    void clear() {
        std::scoped_lock lock(mutex_);
        items_.clear();
    }

private:
    const std::size_t capacity_;
    const QueueOverflowPolicy overflow_policy_;
    mutable std::mutex mutex_;
    std::deque<T> items_;
};

}  // namespace unkxwyno::core

