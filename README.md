# Multififo

The multififo is a concurrent, relaxed FIFO queue based on the MultiQueue.
The idea is that internally there are `c*p` FIFO queues, where `p` is the
number of threads and `c` is a tuning parameter. Every element is tagged with
the time of its insertion. Elements are inserted into a random queue while
deletions delete the oldest element of two random queues.

The queues are bounded ring buffers, `try_push` fails only if (nearly) all
queues are full.

## Modes

The mode (`multififo/modes/`) selects the queues of a handle, the same modes
as in the MultiQueue are available:

- `Random`: new random queues for every operation (`pop_stale = false`
  retries if the oldest element changed between selection and locking)
- `StickRandom` (default): keep the selection for `stickiness` operations,
  reselect all queues on contention
- `StickReplace`: like `StickRandom`, but only replace the contended queue
- `StickMark`: like `StickReplace`, but mark the selected queues so other
  handles avoid them
- `StickSwap`: handles swap their queues with each other on contention

# Usage

Clone the repository with
```bash
git clone https://github.com/marvinwilliams/multiqueue
git switch multififo
```

You have two options on how to use this library
## (a) Install
Then you can either build and install with
```bash
cmake -B build
cmake --build build
cmake --install build --prefix <prefix>
```

and include it in your cmake project with
```cmake
find_package(multififo)
target_link_libaries(target PRIVATE multififo::multififo)
```

## (b) Subdirectory
Copy the project into your source tree and include it as a subdirectory in cmake
```cmake
add_subdirectory(multififo)
target_link_libraries(target PRIVATE multififo::multififo)
```

# Remarks

The implementation is subject of experimantation and thus has more
customization points than practically desireable.
