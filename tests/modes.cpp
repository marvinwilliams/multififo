#include "multififo/multififo.hpp"

#include "multififo/modes/random.hpp"
#include "multififo/modes/stick_mark.hpp"
#include "multififo/modes/stick_random.hpp"
#include "multififo/modes/stick_replace.hpp"
#include "multififo/modes/stick_swap.hpp"

#include "catch2/catch_template_test_macros.hpp"
#include "catch2/catch_test_macros.hpp"
#include "catch2/generators/catch_generators.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ratio>
#include <thread>
#include <vector>

namespace {

constexpr int num_pop_candidates = 2;

using ModeRandom = multififo::mode::Random<num_pop_candidates>;
using ModeRandomStrict = multififo::mode::Random<num_pop_candidates, false>;
using ModeStickRandom = multififo::mode::StickRandom<num_pop_candidates>;
using ModeStickMark = multififo::mode::StickMark<num_pop_candidates>;
using ModeStickReplace = multififo::mode::StickReplace<num_pop_candidates>;
using ModeStickSwap = multififo::mode::StickSwap<num_pop_candidates>;

template <typename Mode>
struct TestPolicy {
    using mode_type = Mode;
    static constexpr int pop_tries = 1;
    static constexpr bool scan = true;
};

// Strictly increasing ticks, so that the order of the elements is well-defined
struct CountingClock {
    using rep = std::uint64_t;
    using period = std::nano;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<CountingClock>;
    static constexpr bool is_steady = true;

    static time_point now() noexcept {
        static std::atomic<rep> count{0};
        return time_point{duration{count.fetch_add(1, std::memory_order_relaxed) + 1}};
    }
};

template <typename Mode>
using test_mf = multififo::MultiFifo<unsigned, TestPolicy<Mode>, CountingClock>;

constexpr std::size_t queues_for(std::size_t num_handles) {
    return num_handles * static_cast<std::size_t>(num_pop_candidates);
}

}  // namespace

#define MODE_LIST ModeRandom, ModeRandomStrict, ModeStickRandom, ModeStickMark, ModeStickReplace, ModeStickSwap

TEMPLATE_TEST_CASE("mode pops every pushed element exactly once", "[modes][conservation]", MODE_LIST) {
    // One handle on as many queues as it may have, and one on more queues than
    // its selection, which exercises reselection and replacement
    std::size_t const num_queues = GENERATE(queues_for(1), queues_for(8));
    CAPTURE(num_queues);
    static constexpr unsigned num_elements = 5000;

    test_mf<TestType> mf(num_queues, num_elements);
    auto handle = mf.get_handle();
    for (unsigned i = 1; i <= num_elements; ++i) {
        REQUIRE(handle.try_push(i));
    }

    std::vector<int> seen(num_elements + 1, 0);
    unsigned popped = 0;
    while (auto v = handle.try_pop()) {
        REQUIRE(*v >= 1);
        REQUIRE(*v <= num_elements);
        ++seen[*v];
        ++popped;
    }
    REQUIRE(popped == num_elements);
    REQUIRE(std::all_of(seen.begin() + 1, seen.end(), [](int c) { return c == 1; }));
}

TEMPLATE_TEST_CASE("mode pops in FIFO order if it compares all queues", "[modes][order]", MODE_LIST) {
    // With as many queues as pop candidates, every pop compares the oldest elements of all queues
    static constexpr unsigned num_elements = 1000;

    test_mf<TestType> mf(queues_for(1), num_elements);
    auto handle = mf.get_handle();
    for (unsigned i = 1; i <= num_elements; ++i) {
        REQUIRE(handle.try_push(i));
    }
    for (unsigned i = 1; i <= num_elements; ++i) {
        auto v = handle.try_pop();
        REQUIRE(v.has_value());
        REQUIRE(*v == i);
    }
    REQUIRE_FALSE(handle.try_pop().has_value());
}

TEMPLATE_TEST_CASE("mode fills all queues before a push fails", "[modes][capacity]", MODE_LIST) {
    static constexpr std::size_t cap_per_queue = 16;
    std::size_t const num_queues = GENERATE(queues_for(1), queues_for(8));
    CAPTURE(num_queues);
    auto const capacity = static_cast<unsigned>(num_queues * cap_per_queue);

    test_mf<TestType> mf(num_queues, cap_per_queue);
    auto handle = mf.get_handle();
    for (unsigned i = 1; i <= capacity; ++i) {
        REQUIRE(handle.try_push(i));
    }
    REQUIRE_FALSE(handle.try_push(capacity + 1));

    // Popping makes room again
    REQUIRE(handle.try_pop().has_value());
    REQUIRE(handle.try_push(capacity + 1));
    REQUIRE_FALSE(handle.try_push(capacity + 2));
}

TEMPLATE_TEST_CASE("mode reports empty on a fresh queue", "[modes][empty]", MODE_LIST) {
    test_mf<TestType> mf(queues_for(1), 16);
    auto handle = mf.get_handle();
    REQUIRE_FALSE(handle.try_pop().has_value());
}

TEMPLATE_TEST_CASE("mode loses no elements under concurrent push and pop", "[modes][concurrent]", MODE_LIST) {
    static constexpr int num_threads = 4;
    static constexpr unsigned per_thread = 2000;
    static constexpr unsigned num_elements = num_threads * per_thread;

    test_mf<TestType> mf(queues_for(num_threads), num_elements);
    std::vector<std::atomic_int> seen(num_elements + 1);
    std::atomic<unsigned> remaining{num_elements};
    std::atomic<unsigned> failed_pushes{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&mf, &seen, &remaining, &failed_pushes, t] {
            auto handle = mf.get_handle();
            for (unsigned i = 0; i < per_thread; ++i) {
                if (!handle.try_push(static_cast<unsigned>(t) * per_thread + i + 1)) {
                    failed_pushes.fetch_add(1, std::memory_order_relaxed);
                    remaining.fetch_sub(1, std::memory_order_relaxed);
                }
            }
            while (remaining.load(std::memory_order_relaxed) > 0) {
                if (auto v = handle.try_pop()) {
                    seen[*v].fetch_add(1, std::memory_order_relaxed);
                    remaining.fetch_sub(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    REQUIRE(failed_pushes.load() == 0);
    std::size_t missing = 0;
    std::size_t duplicated = 0;
    for (unsigned i = 1; i <= num_elements; ++i) {
        int const count = seen[i].load(std::memory_order_relaxed);
        if (count == 0) {
            ++missing;
        } else if (count > 1) {
            ++duplicated;
        }
    }
    REQUIRE(missing == 0);
    REQUIRE(duplicated == 0);
}

TEST_CASE("sticky config reaches the mode", "[modes][config]") {
    using mf_type = test_mf<ModeStickRandom>;
    mf_type::config_type config{};
    config.seed = 7;
    config.stickiness = 3;
    mf_type mf(queues_for(1), 16, config);
    REQUIRE(mf.config().seed == 7);
    REQUIRE(mf.config().stickiness == 3);
}
