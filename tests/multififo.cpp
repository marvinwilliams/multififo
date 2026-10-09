#include "multififo/multififo.hpp"

#include "catch2/catch_test_macros.hpp"

#include <cstddef>
#include <optional>
#include <type_traits>

namespace {

struct StubMode {
    struct config_type {
        int seed{1};
    };

    struct shared_data_type {
        explicit shared_data_type(std::size_t /*num_queues*/) noexcept {
        }
    };

   protected:
    StubMode(config_type const& /*config*/, shared_data_type& /*shared_data*/) noexcept {
    }

    template <typename Context>
    bool push(Context& /*ctx*/, typename Context::value_type const& /*v*/) {
        return false;
    }

    template <typename Context>
    std::optional<typename Context::value_type> try_pop(Context& /*ctx*/) {
        return std::nullopt;
    }
};

struct StubPolicy {
    using mode_type = StubMode;
    static constexpr int pop_tries = 1;
    static constexpr bool scan = true;
};

using mf_type = multififo::MultiFifo<unsigned, StubPolicy>;
using config_type = mf_type::config_type;

}  // namespace

static_assert(std::is_constructible_v<mf_type, std::size_t, std::size_t>);
static_assert(std::is_constructible_v<mf_type, std::size_t, std::size_t, config_type>);
static_assert(!std::is_constructible_v<mf_type, std::size_t, config_type>);

TEST_CASE("multififo takes a braced config after the number of queues and their capacity",
          "[multififo][constructor]") {
    int const seed = 3;
    mf_type mf(16, 64, {seed});
    REQUIRE(mf.num_queues() == 16);
    REQUIRE(mf.config().seed == seed);
}

TEST_CASE("multififo rounds the capacity per queue up to a power of two", "[multififo][capacity]") {
    // The stub mode always fails, so every element goes through the scan
    mf_type mf(3, 5);
    auto handle = mf.get_handle();
    unsigned pushed = 0;
    while (handle.try_push(pushed + 1)) {
        ++pushed;
    }
    REQUIRE(pushed == 3 * 8);
    unsigned popped = 0;
    while (handle.try_pop()) {
        ++popped;
    }
    REQUIRE(popped == pushed);
}
