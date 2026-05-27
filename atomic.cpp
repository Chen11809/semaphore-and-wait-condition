// ---------------------------------------------------------------------------
// atomic.cpp
//
// Same producer/consumer problem as wait_condition.cpp / semaphore.cpp, this
// time with no mutex and no semaphore. Just two std::atomic<uint64_t>
// counters acting as the head and tail of a single-producer/single-consumer
// (SPSC) ring buffer. This is the canonical Lamport SPSC queue.
//
// Bookkeeping
// -----------
//   * 'head' is the TOTAL number of items the producer has ever written.
//     Only the producer ever STORES to it; both threads load it.
//
//   * 'tail' is the TOTAL number of items the consumer has ever read.
//     Only the consumer ever STORES to it; both threads load it.
//
//   * Slot for the k-th item is buffer[k % BufferSize] - same indexing
//     trick as the other two implementations.
//
//   * Invariant at every observable moment:
//         0 <= head - tail <= BufferSize
//     - head - tail == 0          -> buffer empty, consumer must wait
//     - head - tail == BufferSize -> buffer full,  producer must wait
//
// Why this works without any lock
// -------------------------------
// With exactly ONE producer and ONE consumer, each counter has a single
// writer, so neither needs a CAS or any read-modify-write. A plain
// release-store publishes; a plain acquire-load observes. The cross-thread
// communication is one-way per counter, which is what makes the protocol
// genuinely lock-free.
//
// The slot-collision argument is the same as in wait_condition.cpp: when
// the producer is touching buffer[h % BufferSize] and the consumer is
// touching buffer[t % BufferSize], we always have
//     1 <= h - t <= BufferSize - 1,
// so the two modulo-indices cannot coincide (a multiple of BufferSize
// cannot fit strictly between 0 and BufferSize).
//
// Memory ordering - the load-bearing part
// ---------------------------------------
//   * Producer does its buffer write FIRST, then publishes head with
//     std::memory_order_release. Any consumer load of head with
//     memory_order_acquire that observes the new value also observes the
//     buffer write - that is the release/acquire happens-before edge.
//
//   * Symmetric on the consumer side: consumer reads its buffer slot, then
//     release-stores the new tail. The producer's acquire-load of tail
//     picks up the "I am done with that slot" signal, which is what makes
//     overwriting buffer[h % BufferSize] safe on the next wrap.
//
//   * Each thread loads ITS OWN counter with memory_order_relaxed: it is
//     the only writer, so there is no value to synchronize WITH.
//
// Parking without a mutex
// -----------------------
// C++20 added std::atomic<T>::wait(old) / notify_one(). wait() blocks the
// caller until the atomic's value is observed to differ from `old`;
// notify_one() wakes one waiter. The underlying kernel primitive is the
// same one std::mutex uses (futex on Linux, WaitOnAddress on Windows), so
// we get the parking semantics without paying for a mutex object.
//
// Compared to the other two implementations
// -----------------------------------------
//   * No std::mutex, no std::condition_variable, no std::counting_semaphore.
//   * Two atomic<uint64_t> counters do all the bookkeeping.
//   * Memory order, not locks, enforces the happens-before relations.
//
// Trade-off: the entire argument hinges on the SPSC assumption. Add a
// second producer or consumer and the plain-store/plain-load trick on the
// counters breaks - two writers would race on the same counter. At that
// point you need a real concurrent queue (Vyukov / Michael-Scott / etc.).
// ---------------------------------------------------------------------------

#include "atomic.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <random>
#include <thread>

namespace {

// --- Tunables -------------------------------------------------------------
// Kept identical to the other two implementations on purpose so the demos
// are directly comparable.
constexpr int DataSize   = 256;
constexpr int BufferSize = 32;

// --- Shared state ---------------------------------------------------------
//
// Monotonic 64-bit counters. We never wrap them in the lifetime of a run
// (DataSize is 256, and even at billions of items per second a 64-bit
// counter takes centuries to overflow), so plain subtraction h - t
// recovers the current occupancy. The modulo down to BufferSize happens
// only at indexing time.
//
// std::atomic<uint64_t> is lock-free on all common 64-bit platforms.
// Correctness only needs atomicity, not lock-freedom, so we don't
// static_assert here - a hypothetical platform that locks would just run
// slower.
std::atomic<std::uint64_t> head{0};
std::atomic<std::uint64_t> tail{0};
char                       buffer[BufferSize];

// --- Helper: thread-local RNG --------------------------------------------
// Same rationale as in the other two files - give the producer its own
// engine so it doesn't contend with anything else for RNG state.
unsigned int randomFourValueIndex()
{
    thread_local std::mt19937                    engine{std::random_device{}()};
    thread_local std::uniform_int_distribution<> dist{0, 3};
    return static_cast<unsigned int>(dist(engine));
}

// --- Producer ------------------------------------------------------------
//
// 1. Load head (we are its only writer, relaxed is sufficient).
// 2. Wait until head - tail < BufferSize. While waiting, park on `tail`
//    via std::atomic::wait - the consumer wakes us with tail.notify_one().
// 3. Write the byte. No lock needed: 1 <= h - t < BufferSize implies the
//    slot h % BufferSize is not currently being touched by the consumer.
// 4. Publish the write: release-store head + 1, then notify any consumer
//    that may be parked on head.
void producer()
{
    for (int i = 0; i < DataSize; ++i) {
        const auto h = head.load(std::memory_order_relaxed);

        auto t = tail.load(std::memory_order_acquire);
        while (h - t == static_cast<std::uint64_t>(BufferSize)) {
            // wait(old) blocks until the observed value differs from `old`.
            // If tail has already moved by the time we call wait, it returns
            // immediately and we re-check the predicate.
            tail.wait(t, std::memory_order_acquire);
            t = tail.load(std::memory_order_acquire);
        }

        buffer[h % BufferSize] = "ACGT"[randomFourValueIndex()];

        // release pairs with the consumer's acquire-load of head: observing
        // h+1 in head implies the buffer write above is also visible.
        head.store(h + 1, std::memory_order_release);
        head.notify_one();
    }
}

// --- Consumer ------------------------------------------------------------
//
// Exact mirror of producer().
void consumer()
{
    for (int i = 0; i < DataSize; ++i) {
        const auto t = tail.load(std::memory_order_relaxed);

        auto h = head.load(std::memory_order_acquire);
        while (h == t) {
            head.wait(h, std::memory_order_acquire);
            h = head.load(std::memory_order_acquire);
        }

        std::fputc(buffer[t % BufferSize], stderr);

        tail.store(t + 1, std::memory_order_release);
        tail.notify_one();
    }
    std::fputc('\n', stderr);
}

} // namespace

void runAtomic()
{
    // Reset in case this function is ever called more than once in the same
    // process (same defensive move as the other two implementations).
    head.store(0, std::memory_order_relaxed);
    tail.store(0, std::memory_order_relaxed);

    std::thread p(producer);
    std::thread c(consumer);
    p.join();
    c.join();
}
