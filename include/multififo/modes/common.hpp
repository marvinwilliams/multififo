#pragma once

#include "multififo/queue_guard.hpp"

#include "pcg_random.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <random>

namespace multififo::mode {

// A mode selects the queues a handle pushes to and pops from. It is a base class of the handle and provides
//   bool push(Context&, value_type const&): false if the selected queue is full (the handle then scans all queues)
//   std::optional<value_type> try_pop(Context&)

struct BaseConfig {
    int seed{1};
};

struct StickyConfig : BaseConfig {
    int stickiness{16};
};

template <int NumPopCandidates>
struct BaseSharedData {
    std::atomic_uint id_count{0};

    explicit BaseSharedData(std::size_t /*num_queues*/) noexcept {
    }

    [[nodiscard]] unsigned next_id() noexcept {
        return id_count.fetch_add(1, std::memory_order_relaxed);
    }
};

template <typename Context, std::size_t N>
[[nodiscard]] std::array<typename Context::tick_type, N> top_ticks(Context const& ctx,
                                                                   std::array<std::size_t, N> const& indices) noexcept {
    std::array<typename Context::tick_type, N> ticks{};
    for (std::size_t i = 0; i < N; ++i) {
        ticks[i] = ctx.queue_guards()[indices[i]].top_tick();
    }
    return ticks;
}

// The position of the queue with the oldest element (empty queues have the largest tick)
template <typename Tick, std::size_t N>
[[nodiscard]] std::size_t best_position(std::array<Tick, N> const& ticks) noexcept {
    std::size_t best = 0;
    for (std::size_t i = 1; i < N; ++i) {
        if (ticks[i] < ticks[best]) {
            best = i;
        }
    }
    return best;
}

template <int NumPopCandidates>
class ModeBase {
    static_assert(NumPopCandidates > 0);

    std::uint32_t id_{};
    pcg32 rng_{};

   public:
    using config_type = BaseConfig;
    using shared_data_type = BaseSharedData<NumPopCandidates>;

   protected:
    using index_array = std::array<std::size_t, static_cast<std::size_t>(NumPopCandidates)>;

    explicit ModeBase(config_type const& config, shared_data_type& shared_data) noexcept
        : id_{shared_data.next_id()} {
        auto seq = std::seed_seq{config.seed, static_cast<int>(id_)};
        rng_.seed(seq);
    }

    [[nodiscard]] std::uint32_t id() const noexcept {
        return id_;
    }

    [[nodiscard]] pcg32& rng() noexcept {
        return rng_;
    }

    [[nodiscard]] std::size_t random_index(std::size_t num_queues) noexcept {
        return std::uniform_int_distribution<std::size_t>{0, num_queues - 1}(rng_);
    }

    [[nodiscard]] std::size_t random_candidate() noexcept {
        return rng_() % static_cast<std::size_t>(NumPopCandidates);
    }

    [[nodiscard]] index_array sample_indices(std::size_t num_queues) noexcept {
        index_array indices{};
        for (auto it = indices.begin(); it != indices.end(); ++it) {
            do {
                *it = random_index(num_queues);
            } while (std::find(indices.begin(), it, *it) != it);
        }
        return indices;
    }

    template <typename Context>
    [[nodiscard]] bool push_random(Context& ctx, typename Context::value_type const& v) {
        while (true) {
            auto& guard = ctx.queue_guards()[random_index(ctx.num_queues())];
            switch (guard.try_push(v)) {
                case PushResult::success:
                    return true;
                case PushResult::full:
                    return false;
                case PushResult::locked:
                    break;
            }
        }
    }
};

template <int NumPopCandidates>
class StickyModeBase : public ModeBase<NumPopCandidates> {
    using base_type = ModeBase<NumPopCandidates>;

    int stickiness_{};
    int remaining_{0};

   public:
    using config_type = StickyConfig;
    using shared_data_type = typename base_type::shared_data_type;

   protected:
    typename base_type::index_array pop_index_{};

    explicit StickyModeBase(config_type const& config, shared_data_type& shared_data) noexcept
        : base_type{config, shared_data}, stickiness_{config.stickiness} {
        assert(stickiness_ > 0);
    }

    [[nodiscard]] int stickiness() const noexcept {
        return stickiness_;
    }

    [[nodiscard]] bool expired() const noexcept {
        return remaining_ <= 0;
    }

    void renew() noexcept {
        remaining_ = stickiness_;
    }

    void expire() noexcept {
        remaining_ = 0;
    }

    void consume() noexcept {
        --remaining_;
    }

    template <typename Context>
    void reselect(Context const& ctx) noexcept {
        pop_index_ = this->sample_indices(ctx.num_queues());
        renew();
    }

    template <typename Context>
    void reselect_if_expired(Context const& ctx) noexcept {
        if (expired()) {
            reselect(ctx);
        }
    }

    template <typename Context>
    void replace(Context const& ctx, std::size_t position) noexcept {
        assert(position < pop_index_.size());
        assert(ctx.num_queues() >= pop_index_.size());
        pop_index_[position] = static_cast<std::size_t>(-1);
        std::size_t index{};
        do {
            index = this->random_index(ctx.num_queues());
        } while (std::find(pop_index_.begin(), pop_index_.end(), index) != pop_index_.end());
        pop_index_[position] = index;
    }
};

}  // namespace multififo::mode
