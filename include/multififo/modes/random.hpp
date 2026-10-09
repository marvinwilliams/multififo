#pragma once

#include "multififo/modes/common.hpp"

#include <optional>

namespace multififo::mode {

template <int num_pop_candidates = 2, bool pop_stale = true>
class Random : public ModeBase<num_pop_candidates> {
    using base_type = ModeBase<num_pop_candidates>;

   public:
    using config_type = typename base_type::config_type;
    using shared_data_type = typename base_type::shared_data_type;

   protected:
    explicit Random(config_type const& config, shared_data_type& shared_data) noexcept
        : base_type{config, shared_data} {
    }

    template <typename Context>
    std::optional<typename Context::value_type> try_pop(Context& ctx) {
        while (true) {
            auto indices = this->sample_indices(ctx.num_queues());
            auto ticks = top_ticks(ctx, indices);
            auto best_pos = best_position(ticks);
            auto& guard = ctx.queue_guards()[indices[best_pos]];
            if (!guard.try_lock()) {
                continue;
            }
            if (guard.get_queue().empty()) {
                guard.unlock();
                return std::nullopt;
            }
            if (!pop_stale && guard.get_queue().top().tick != ticks[best_pos]) {
                guard.unlock();
                continue;
            }
            auto v = guard.pop_locked();
            guard.unlock();
            return v;
        }
    }

    template <typename Context>
    bool push(Context& ctx, typename Context::value_type const& v) {
        return this->push_random(ctx, v);
    }
};

}  // namespace multififo::mode
