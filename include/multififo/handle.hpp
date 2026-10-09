#pragma once

#include "multififo/queue_guard.hpp"

#include <optional>

namespace multififo {

template <typename Context>
class Handle : public Context::policy_type::mode_type {
    using mode_type = typename Context::policy_type::mode_type;

    Context *context_;

   public:
    using value_type = typename Context::value_type;

    explicit Handle(Context &ctx) noexcept : mode_type{ctx.config(), ctx.shared_data()}, context_{&ctx} {
    }

    Handle(Handle const &) = delete;
    Handle(Handle &&) noexcept = default;
    Handle &operator=(Handle const &) = delete;
    Handle &operator=(Handle &&) noexcept = default;
    ~Handle() = default;

    // Pushes to the first queue that is neither locked nor full
    bool scan_push(value_type const &v) {
        for (auto *it = context_->queue_guards(); it != context_->queue_guards() + context_->num_queues(); ++it) {
            if (it->try_push(v) == PushResult::success) {
                return true;
            }
        }
        return false;
    }

    std::optional<value_type> scan_pop() {
        for (auto *it = context_->queue_guards(); it != context_->queue_guards() + context_->num_queues(); ++it) {
            if (!it->try_lock()) {
                continue;
            }
            auto v = it->pop_locked();
            it->unlock();
            if (v) {
                return v;
            }
        }
        return std::nullopt;
    }

    // Fails only if the queues are (close to) full
    bool try_push(value_type const &v) {
        if (mode_type::push(*context_, v)) {
            return true;
        }
        return scan_push(v);
    }

    std::optional<value_type> try_pop() {
        for (int i = 0; i < Context::policy_type::pop_tries; ++i) {
            std::optional<value_type> v = mode_type::try_pop(*context_);
            if (v) {
                return v;
            }
        }
        if (!Context::policy_type::scan) {
            return std::nullopt;
        }
        return scan_pop();
    }
};

}  // namespace multififo
