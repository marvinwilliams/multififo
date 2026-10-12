# Multififo

The multififo is a concurrent, relaxed FIFO queue based on the MultiQueue.
The idea is that internally there are `c*p` FIFO queues, where `p` is the
number of threads and `c` is a tuning parameter. Every element is tagged with
the time of its insertion. Elements are inserted into a random queue while
deletions delete the oldest element of two random queues.

The queues are bounded ring buffers, `try_push` fails only if (nearly) all
queues are full.

# Usage
Copy the project into your source tree and include it as a subdirectory in cmake
```cmake
add_subdirectory(multififo)
target_link_libraries(target PRIVATE multififo::multififo)
```

# Remarks

The implementation is subject of experimantation and thus has more
customization points than practically desireable.
