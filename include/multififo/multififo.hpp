/**
******************************************************************************
* @file:   multififo.hpp
*
* @author: Marvin Williams
* @date:   2021/03/29 17:19
* @brief:
*******************************************************************************
**/
#pragma once

#include "multififo/handle.hpp"
#include "multififo/modes/stick_random.hpp"
#include "multififo/queue_guard.hpp"
#include "multififo/ring_buffer.hpp"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace multififo {

struct DefaultPolicy {
    using mode_type = mode::StickRandom<>;
    static constexpr int pop_tries = 1;
    static constexpr bool scan = true;
};

// Every queue is a bounded ring buffer whose elements carry the tick of their push. Pops take the oldest element of
// the queues selected by the mode, comparing the ticks of their oldest elements.
template <typename T, typename Policy = DefaultPolicy, typename Clock = std::chrono::steady_clock,
          typename Allocator = std::allocator<T>>
class MultiFifo {
   public:
    using value_type = T;
    using reference = value_type &;
    using const_reference = value_type const &;
    using size_type = std::size_t;
    using policy_type = Policy;
    using clock_type = Clock;
    using allocator_type = Allocator;
    using config_type = typename policy_type::mode_type::config_type;

   private:
    struct Element {
        std::uint64_t tick;
        value_type value;
    };
    using queue_type = RingBuffer<Element>;
    using guard_type = QueueGuard<queue_type, clock_type>;
    // Allocating the guards as guard_type (instead of raw bytes) respects their cache-line alignment
    using internal_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<guard_type>;

    static constexpr size_type round_up_po2(size_type n) noexcept {
        size_type po2 = 1;
        while (po2 < n) {
            po2 *= 2;
        }
        return po2;
    }

    class Context {
        friend MultiFifo;

       public:
        using value_type = MultiFifo::value_type;
        using policy_type = MultiFifo::policy_type;
        using guard_type = MultiFifo::guard_type;
        using tick_type = typename guard_type::tick_type;
        using shared_data_type = typename policy_type::mode_type::shared_data_type;

       private:
        [[no_unique_address]] internal_allocator_type alloc_;
        size_type num_queues_{};
        guard_type *queue_guards_{nullptr};
        [[no_unique_address]] config_type config_;
        [[no_unique_address]] shared_data_type data_;

        explicit Context(size_type num_queues, typename queue_type::size_type cap_per_queue, config_type const &config,
                         internal_allocator_type const &alloc)
            : alloc_{alloc},
              num_queues_{num_queues},
              queue_guards_{std::allocator_traits<internal_allocator_type>::allocate(alloc_, num_queues_)},
              config_{config},
              data_{num_queues_} {
            assert(num_queues_ > 0);
            assert(reinterpret_cast<std::uintptr_t>(queue_guards_) % alignof(guard_type) == 0);
            for (auto *it = queue_guards_; it != queue_guards_ + num_queues_; ++it) {
                std::allocator_traits<internal_allocator_type>::construct(alloc_, it, queue_type{cap_per_queue});
            }
        }

        ~Context() noexcept {
            for (auto *it = queue_guards_; it != queue_guards_ + num_queues_; ++it) {
                std::allocator_traits<internal_allocator_type>::destroy(alloc_, it);
            }
            std::allocator_traits<internal_allocator_type>::deallocate(alloc_, queue_guards_, num_queues_);
        }

       public:
        Context(const Context &) = delete;
        Context(Context &&) = delete;
        Context &operator=(const Context &) = delete;
        Context &operator=(Context &&) = delete;

        [[nodiscard]] constexpr size_type num_queues() const noexcept {
            return num_queues_;
        }

        [[nodiscard]] guard_type *queue_guards() const noexcept {
            return queue_guards_;
        }

        [[nodiscard]] config_type const &config() const noexcept {
            return config_;
        }

        [[nodiscard]] shared_data_type &shared_data() noexcept {
            return data_;
        }

        [[nodiscard]] shared_data_type const &shared_data() const noexcept {
            return data_;
        }
    };

    Context context_;

   public:
    using handle_type = Handle<Context>;

    // The capacity of every queue is rounded up to a power of two
    explicit MultiFifo(size_type num_queues, size_type cap_per_queue, config_type const &config = {},
                       allocator_type const &alloc = {})
        : context_{num_queues, round_up_po2(cap_per_queue), config, internal_allocator_type(alloc)} {
    }

    handle_type get_handle() noexcept {
        return handle_type(context_);
    }

    [[nodiscard]] constexpr size_type num_queues() const noexcept {
        return context_.num_queues_;
    }

    [[nodiscard]] allocator_type get_allocator() const {
        return allocator_type(context_.alloc_);
    }

    [[nodiscard]] config_type const &config() const {
        return context_.config();
    }
};

}  // namespace multififo
