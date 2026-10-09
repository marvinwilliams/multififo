/**
******************************************************************************
* @file:   queue_guard.hpp
*
* @author: Marvin Williams
* @date:   2021/11/09 18:05
* @brief:
*******************************************************************************
**/

#pragma once

#include "multififo/build_config.hpp"

#include <atomic>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace multififo {

enum class PushResult { success, locked, full };

// Guards a bounded FIFO queue of (tick, value) elements. The tick of the oldest element is published so that the
// modes can select the queue with the oldest element without locking. Ticks are taken from `Clock`.
template <typename Queue, typename Clock>
class alignas(build_config::l1_cache_line_size) QueueGuard {
   public:
    using queue_type = Queue;
    using clock_type = Clock;
    using element_type = typename queue_type::value_type;
    using value_type = decltype(std::declval<element_type>().value);
    using tick_type = std::uint64_t;

    static constexpr tick_type empty_tick = std::numeric_limits<tick_type>::max();

   private:
    std::atomic<tick_type> top_tick_ = empty_tick;
    std::atomic_uint32_t lock_ = 0;
    queue_type queue_;

   public:
    explicit QueueGuard(queue_type queue) noexcept : queue_(std::move(queue)) {
    }

    [[nodiscard]] tick_type top_tick() const noexcept {
        return top_tick_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool empty() const noexcept {
        return top_tick() == empty_tick;
    }

    void popped() {
        auto tick = (queue_.empty() ? empty_tick : queue_.top().tick);
        top_tick_.store(tick, std::memory_order_relaxed);
    }

    void pushed() {
        auto tick = queue_.top().tick;
        if (tick != top_tick()) {
            top_tick_.store(tick, std::memory_order_relaxed);
        }
    }

    [[nodiscard]] std::optional<value_type> pop_locked() {
        if (queue_.empty()) {
            return std::nullopt;
        }
        auto v = queue_.top().value;
        queue_.pop();
        popped();
        return v;
    }

    // The tick is taken while holding the lock, so the ticks in every queue are increasing
    [[nodiscard]] bool push_locked(value_type const& v) {
        if (queue_.full()) {
            return false;
        }
        queue_.push({static_cast<tick_type>(clock_type::now().time_since_epoch().count()), v});
        pushed();
        return true;
    }

    bool try_lock() noexcept {
        // Test first to not invalidate the cache line
        return (lock_.load(std::memory_order_relaxed) & 1U) == 0U &&
            (lock_.exchange(1U, std::memory_order_acquire) & 1U) == 0U;
    }

    bool try_lock_if_marked(std::uint32_t mark) noexcept {
        auto current = lock_.load(std::memory_order_relaxed);
        std::uint32_t marked = mark << 1;
        return (current == marked || current == 0U) &&
            lock_.compare_exchange_strong(current, marked | 1U, std::memory_order_acquire, std::memory_order_relaxed);
    }

    [[nodiscard]] PushResult try_push(value_type const& v) {
        if (!try_lock()) {
            return PushResult::locked;
        }
        bool success = push_locked(v);
        unlock();
        return success ? PushResult::success : PushResult::full;
    }

    void unlock() {
        lock_.store(0U, std::memory_order_release);
    }

    void unlock_marked() noexcept {
        lock_.fetch_and(~1U, std::memory_order_release);
    }

    void set_mark(std::uint32_t mark) noexcept {
        auto current = lock_.load(std::memory_order_relaxed);
        auto old_mark = current >> 1;
        while (!lock_.compare_exchange_weak(current, (mark << 1) | (current & 1U), std::memory_order_relaxed)) {
            if ((current >> 1) != old_mark) {
                return;
            }
        }
    }

    queue_type& get_queue() noexcept {
        return queue_;
    }
};

}  // namespace multififo
