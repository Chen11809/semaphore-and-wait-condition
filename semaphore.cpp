// ---------------------------------------------------------------------------
// semaphore.cpp
//
// Same producer/consumer problem as wait_condition.cpp, but solved using a
// pair of counting semaphores instead of a mutex + two condition variables.
// This is the textbook "bounded buffer with two semaphores" pattern and is
// the cleanest C++20 expression of it.
//
// Why no mutex here?
// ------------------
// With ONE producer and ONE consumer, the two semaphores already encode
// every invariant we need:
//
//   * 'freeSlots' counts how many slots are currently EMPTY. The producer
//     must acquire one before it is allowed to write. Initial value:
//     BufferSize (the whole buffer starts empty).
//
//   * 'usedSlots' counts how many slots are currently FILLED. The consumer
//     must acquire one before it is allowed to read. Initial value: 0
//     (the buffer starts empty, so nothing to read).
//
// The producer's only blocking point is freeSlots.acquire() and the
// consumer's only blocking point is usedSlots.acquire(). Each acquire is
// paired with a release on the OTHER semaphore, so the buffer's occupancy
// always stays in [0, BufferSize] and the two threads cannot collide on
// the same slot: at any instant a given slot is either free (so only the
// producer can be there) or used (so only the consumer can be there).
//
// Compared to wait_condition.cpp:
//
//   * No std::mutex.
//   * No condition variables.
//   * No predicate. The semaphore count IS the predicate.
//   * Roughly half the lines, and the data flow is obvious at a glance.
//
// The trade-off: condition variables generalize to arbitrary predicates
// (e.g. "wake when the high-water mark drops below 70%"), while semaphores
// only count. For pure bounded-buffer flow control, semaphores win.
// ---------------------------------------------------------------------------

#include "semaphore.h"

#include <cstdio>
#include <random>
#include <semaphore>
#include <thread>

namespace {

// --- Tunables -------------------------------------------------------------
//
// Kept identical to wait_condition.cpp on purpose so the two demos are
// directly comparable.
constexpr int DataSize   = 256;
constexpr int BufferSize = 32;

// --- Shared state ---------------------------------------------------------
//
// std::counting_semaphore<MaxCount> is a non-copyable, non-movable
// synchronization primitive added in C++20 (header <semaphore>). The
// template argument is the maximum count the semaphore can hold; using
// BufferSize is the natural choice because no slot can be "more than
// completely free" or "more than completely used" simultaneously.
//
// The constructor argument is the INITIAL count:
//   - freeSlots starts at BufferSize: every slot is initially free, so the
//     producer can run BufferSize times before it has to wait.
//   - usedSlots starts at 0: no slot is filled yet, so the consumer must
//     wait for the producer's first release.
std::counting_semaphore<BufferSize> freeSlots{BufferSize};
std::counting_semaphore<BufferSize> usedSlots{0};
char                                buffer[BufferSize];

// --- Helper: thread-local RNG --------------------------------------------
//
// Same rationale as in wait_condition.cpp - give the producer its own
// engine so it does not contend with anything else for RNG state.
unsigned int randomFourValueIndex()
{
    thread_local std::mt19937                    engine{std::random_device{}()};
    thread_local std::uniform_int_distribution<> dist{0, 3};
    return static_cast<unsigned int>(dist(engine));
}

// --- Producer ------------------------------------------------------------
//
// The protocol is symmetric and very short:
//
//   1. acquire a free slot (block until one is available)
//   2. write the byte
//   3. release a used slot (wake the consumer if it was waiting)
//
// Notice there is no buffer-size check and no predicate variable -
// freeSlots.acquire() does the bookkeeping for us.
void producer()
{
    for (int i = 0; i < DataSize; ++i) {
        freeSlots.acquire();                              // (1) wait for a free slot
        buffer[i % BufferSize] = "ACGT"[randomFourValueIndex()]; // (2) write
        usedSlots.release();                              // (3) one more filled slot
    }
}

// --- Consumer ------------------------------------------------------------
//
// Exact mirror of the producer.
void consumer()
{
    for (int i = 0; i < DataSize; ++i) {
        usedSlots.acquire();                              // wait for a filled slot
        std::fputc(buffer[i % BufferSize], stderr);       // read
        freeSlots.release();                              // one more free slot
    }
    std::fputc('\n', stderr);
}

} // namespace

void runSemaphore()
{
    std::thread p(producer);
    std::thread c(consumer);
    p.join();
    c.join();
}
