// ---------------------------------------------------------------------------
// atomic.h
//
// Public entry point for the lock-free SPSC ring-buffer version of the
// producer/consumer demo. Uses only std::atomic operations - no mutex,
// no condition variable, no semaphore. Same external behavior as the other
// two: spawn a producer and a consumer thread, run to completion, print
// DataSize ACGT bytes to stderr followed by a newline.
// ---------------------------------------------------------------------------

#pragma once

void runAtomic();
