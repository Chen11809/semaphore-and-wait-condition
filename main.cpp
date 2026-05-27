// ---------------------------------------------------------------------------
// main.cpp
//
// Tiny CLI dispatcher. Picks between the three producer/consumer
// implementations based on argv:
//
//   semaphore_and_wait_condition --wait-condition
//   semaphore_and_wait_condition --semaphore
//   semaphore_and_wait_condition --atomic
//
// Anything else (no argument, unknown argument, multiple arguments) prints
// a usage line to stderr and exits with status 1.
//
// The interesting code lives in wait_condition.cpp / semaphore.cpp /
// atomic.cpp; this file is intentionally trivial.
// ---------------------------------------------------------------------------

#include "atomic.h"
#include "semaphore.h"
#include "wait_condition.h"

#include <cstdio>
#include <cstring>

namespace {

// Print a one-line usage message to stderr. Kept as a free function so the
// "bad argv" path stays a single return point in main().
void printUsage(const char *programName)
{
    // argv[0] may be empty on exotic platforms; fall back to a stable name.
    const char *name = (programName && *programName) ? programName
                                                     : "semaphore_and_wait_condition";
    std::fprintf(stderr,
                 "usage: %s (--semaphore | --wait-condition | --atomic)\n",
                 name);
}

} // namespace

int main(int argc, char *argv[])
{
    // We expect exactly one flag. Anything else is a usage error.
    if (argc != 2) {
        printUsage(argc > 0 ? argv[0] : nullptr);
        return 1;
    }

    const char *arg = argv[1];

    if (std::strcmp(arg, "--wait-condition") == 0) {
        runWaitCondition();
        return 0;
    }
    if (std::strcmp(arg, "--semaphore") == 0) {
        runSemaphore();
        return 0;
    }
    if (std::strcmp(arg, "--atomic") == 0) {
        runAtomic();
        return 0;
    }

    // Unknown flag.
    printUsage(argv[0]);
    return 1;
}
