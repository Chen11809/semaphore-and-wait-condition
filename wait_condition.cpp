// ---------------------------------------------------------------------------
// wait_condition.cpp
//
// Direct port of the Qt "Wait Conditions" producer/consumer example
// (https://doc.qt.io/qt-6/qtcore-threads-waitconditions-example.html) to the
// C++ standard library.
//
// The structure intentionally mirrors the Qt original line-for-line so the
// mapping between Qt primitives and their std equivalents is obvious:
//
//   +---------------------+-----------------------------------------+
//   | Qt                  | std                                     |
//   +---------------------+-----------------------------------------+
//   | QMutex              | std::mutex                              |
//   | QMutexLocker        | std::lock_guard / std::unique_lock      |
//   | QWaitCondition      | std::condition_variable                 |
//   | cond.wait(&mutex)   | cv.wait(lk, predicate)                  |
//   | cond.wakeAll()      | cv.notify_all()                         |
//   | QThread             | std::thread                             |
//   | QRandomGenerator    | std::mt19937 + uniform_int_distribution |
//   +---------------------+-----------------------------------------+
//
// One small but important shape difference: Qt's example writes the
// CHECK loop manually:
//
//     while (numUsedBytes == BufferSize)
//         bufferNotFull.wait(&mutex);
//
// We use the predicate overload of std::condition_variable::wait, which
// expands to *exactly* the same loop internally and additionally absorbs
// spurious wake-ups. This is the idiomatic std spelling.
// ---------------------------------------------------------------------------

#include "wait_condition.h"

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>

namespace {

// --- Tunables -------------------------------------------------------------
//
// Slim demo values - the Qt original uses DataSize = 100000 and
// BufferSize = 8192. Smaller numbers are easier to inspect and still
// exercise both wait conditions (the buffer wraps DataSize / BufferSize
// times, so the producer is guaranteed to block on "buffer full" and the
// consumer is guaranteed to block on "buffer empty" during the run).
//
// If you want to stress the synchronization, lower BufferSize to e.g. 4
// and watch the producer/consumer hand off more aggressively.
constexpr int DataSize   = 256;
constexpr int BufferSize = 32;

// --- Shared state ---------------------------------------------------------
//
// These globals are exactly the four objects from the Qt example:
//
//   * 'mutex' protects 'numUsedBytes'. It does NOT protect the buffer
//     contents directly - see the comment in the producer below for why
//     that is safe here.
//
//   * 'bufferNotEmpty' is signaled by the producer whenever it has just
//     deposited a byte. The consumer waits on it when the buffer is empty.
//
//   * 'bufferNotFull' is signaled by the consumer whenever it has just
//     consumed a byte. The producer waits on it when the buffer is full.
//
//   * 'numUsedBytes' is the count of filled slots in the ring buffer; it
//     is the predicate variable for both condition variables.
std::mutex              mutex;
std::condition_variable bufferNotEmpty;
std::condition_variable bufferNotFull;
char                    buffer[BufferSize];
int                     numUsedBytes = 0;

// --- Helper: thread-local RNG --------------------------------------------
//
// std::rand() is not thread-safe in a useful way and shares state across
// threads. Give each thread its own mt19937, seeded from std::random_device,
// so the producer can generate ACGT bytes without locking and without
// stepping on the consumer (the consumer does not use the RNG, but the
// pattern is worth showing).
unsigned int randomFourValueIndex()
{
    // Constructed once per thread on first call. thread_local makes the
    // engine and distribution survive across loop iterations without
    // re-seeding.
    thread_local std::mt19937                    engine{std::random_device{}()};
    thread_local std::uniform_int_distribution<> dist{0, 3};
    return static_cast<unsigned int>(dist(engine));
}

// --- Producer ------------------------------------------------------------
//
// The producer fills the buffer with random ACGT characters. Its loop is
// structurally identical to Qt's:
//
//   1. Lock, wait until there is room, unlock.
//   2. Write one byte to buffer[i % BufferSize] *without holding the lock*.
//   3. Lock, bump the count, signal the consumer, unlock.
//
// Why is writing the buffer slot OUTSIDE the lock safe? Because:
//
//   * There is exactly ONE producer and ONE consumer in this example.
//   * The producer only writes to buffer[i % BufferSize] when
//     numUsedBytes < BufferSize. That is, the slot is currently considered
//     FREE - the consumer has either not read it yet for this wrap, or has
//     already finished reading it from the previous wrap.
//   * The consumer only reads buffer[i % BufferSize] when numUsedBytes > 0.
//     That is, the slot is currently considered USED, which only happens
//     after the producer has bumped numUsedBytes for that slot.
//
// The count 'numUsedBytes' therefore acts as a ticket that hands ownership
// of each slot between producer and consumer; the mutex only needs to
// protect the count itself, not the payload. This matches the Qt example's
// shape exactly. (With multiple producers or multiple consumers this
// reasoning would break and the lock would have to span the buffer access
// too.)
void producer()
{
    for (int i = 0; i < DataSize; ++i) {
        {
            std::unique_lock<std::mutex> lk(mutex);

            // Predicate form: equivalent to
            //   while (!(numUsedBytes < BufferSize))
            //       bufferNotFull.wait(lk);
            // but also handles spurious wake-ups by re-checking the
            // predicate before returning.
            bufferNotFull.wait(lk, [] { return numUsedBytes < BufferSize; });
        }

        // Lock released - safe to touch buffer[i % BufferSize] because the
        // slot is logically owned by the producer until we bump the count.
        buffer[i % BufferSize] = "ACGT"[randomFourValueIndex()];

        {
            std::lock_guard<std::mutex> lk(mutex);
            ++numUsedBytes;
            // notify_all mirrors Qt's wakeAll(). With one consumer,
            // notify_one would suffice, but we follow the Qt original.
            bufferNotEmpty.notify_all();
        }
    }
}

// --- Consumer ------------------------------------------------------------
//
// Mirror image of the producer. The std::fputc call goes through the C
// stdio layer, which is itself internally synchronized; writing to stderr
// from one thread at a time is fine.
void consumer()
{
    for (int i = 0; i < DataSize; ++i) {
        {
            std::unique_lock<std::mutex> lk(mutex);
            bufferNotEmpty.wait(lk, [] { return numUsedBytes > 0; });
        }

        std::fputc(buffer[i % BufferSize], stderr);

        {
            std::lock_guard<std::mutex> lk(mutex);
            --numUsedBytes;
            bufferNotFull.notify_all();
        }
    }
    std::fputc('\n', stderr);
}

} // namespace

void runWaitCondition()
{
    // Reset the shared count in case this function is ever called more than
    // once in the same process. (Not used today, but cheap insurance.)
    numUsedBytes = 0;

    std::thread p(producer);
    std::thread c(consumer);

    // Join is the std equivalent of Qt's QThread::wait().
    p.join();
    c.join();
}
