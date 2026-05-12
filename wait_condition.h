// ---------------------------------------------------------------------------
// wait_condition.h
//
// Public entry point for the std::condition_variable port of the Qt
// "Wait Conditions" producer/consumer example.
//
// runWaitCondition() spawns a producer thread and a consumer thread, waits
// for both to finish, and returns. It uses no Qt APIs at all - only
// <thread>, <mutex>, and <condition_variable>.
// ---------------------------------------------------------------------------

#pragma once

void runWaitCondition();
