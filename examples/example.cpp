#include "multififo/multififo.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

using mf_type = multififo::MultiFifo<unsigned>;

static const unsigned int n_threads = 4;
static const std::size_t iterations_per_thread = 1'000;

static void work(mf_type& mf) {
    auto handle = mf.get_handle();
    for (unsigned i = 1; i <= iterations_per_thread; ++i) {
        handle.try_push(i);
        handle.try_pop();
    }
}

int main() {
    std::vector<std::thread> threads;
    mf_type mf(static_cast<std::size_t>(4 * n_threads), 1024);
    std::generate_n(std::back_inserter(threads), n_threads, [&mf]() { return std::thread{work, std::ref(mf)}; });
    std::for_each(threads.begin(), threads.end(), [](auto& t) { t.join(); });
}
