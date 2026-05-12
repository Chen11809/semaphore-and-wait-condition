// ---------------------------------------------------------------------------
// semaphore.h
//
// Public entry point for the std::counting_semaphore version of the same
// producer/consumer problem. Same external behavior as runWaitCondition():
// spawns a producer thread and a consumer thread, runs to completion,
// prints DataSize ACGT bytes to stderr followed by a newline.
// ---------------------------------------------------------------------------

#pragma once

void runSemaphore();
