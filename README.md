# semaphore-and-wait-condition

A small teaching project that reimplements the Qt
[Wait Conditions producer/consumer example][qt-example]
using only the C++ standard library — no Qt — and shows the same problem
solved two more ways: with `std::counting_semaphore`, and lock-free with
just `std::atomic`. The three versions live side-by-side in one
executable so they are easy to compare.

[qt-example]: https://doc.qt.io/qt-6/qtcore-threads-waitconditions-example.html

## What it demonstrates

| Variant         | Primitives                                         | Where to look           |
| --------------- | -------------------------------------------------- | ----------------------- |
| Wait condition  | `std::mutex` + `std::condition_variable` (×2)      | `wait_condition.cpp`    |
| Semaphore       | `std::counting_semaphore` (×2), no mutex needed    | `semaphore.cpp`         |
| Atomic          | `std::atomic<uint64_t>` (×2) + `wait`/`notify`     | `atomic.cpp`            |

All three implementations are a single-producer / single-consumer bounded
ring buffer filled with random ACGT bytes and drained to `stderr`.

A direct Qt → std mapping table is embedded as comments in
`wait_condition.cpp`.

## Requirements

- A C++20 compiler with `<semaphore>`:
  - MSVC 19.29+ (Visual Studio 2019 16.10 or newer / Visual Studio 2022)
  - GCC 11+
  - Clang 14+ with libstdc++ 11+ or libc++ 15+
- CMake 3.20+

## Build

```bash
cmake -S . -B build
cmake --build build
```

On Windows with the default MSVC multi-config generator the executable
lands in `build/Debug/semaphore_and_wait_condition.exe`. On single-config
generators (Ninja, MinGW Makefiles) it lands in
`build/semaphore_and_wait_condition[.exe]`.

### Inside VS Code

The repo ships a small `.vscode/` setup:

- `Ctrl+Shift+B` runs the default build task (which depends on configure).
- `F5` debugs the selected configuration from the Run and Debug sidebar.
  Two pairs of launch configs are provided:
  - `Run --wait-condition / --semaphore (MSVC)` use the static MSVC path.
  - `Run --wait-condition / --semaphore (CMake Tools)` use
    `${command:cmake.launchTargetPath}` — works with any generator if you
    have the CMake Tools extension installed.

IntelliSense is wired up via `.vscode/c_cpp_properties.json` to defer to
CMake Tools (with `c++20` as a fallback) so `std::counting_semaphore` and
friends resolve correctly.

## Run

```text
semaphore_and_wait_condition --wait-condition
semaphore_and_wait_condition --semaphore
semaphore_and_wait_condition --atomic
semaphore_and_wait_condition                    # prints usage, exits 1
```

Each successful run prints 256 ACGT characters to `stderr` followed by a
newline. To stress the synchronization, lower `BufferSize` in either
source file and rebuild — both versions should still print exactly
`DataSize` chars, just with more producer/consumer turn-taking.

## Layout

```
.
├── CMakeLists.txt
├── main.cpp                CLI dispatcher
├── wait_condition.h        runWaitCondition()
├── wait_condition.cpp      std::mutex + std::condition_variable port
├── semaphore.h             runSemaphore()
├── semaphore.cpp           std::counting_semaphore version
├── atomic.h                runAtomic()
├── atomic.cpp              lock-free SPSC ring buffer (std::atomic only)
├── .vscode/
│   ├── tasks.json
│   ├── launch.json
│   └── c_cpp_properties.json
├── .gitignore
├── LICENSE                 MIT
└── README.md
```

## License

[MIT](./LICENSE).
