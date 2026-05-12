# ----------------------------------------------------------------------------
# semaphore-and-wait-condition
#
# A small teaching project that reimplements the Qt "Wait Conditions"
# producer/consumer example using only the C++ standard library, side-by-side
# with a semaphore-based version. Build produces a single executable; the
# user picks which synchronization strategy to run via a command-line flag.
#
#   semaphore_and_wait_condition --wait-condition
#   semaphore_and_wait_condition --semaphore
# ----------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.20)

project(semaphore_and_wait_condition
    LANGUAGES CXX
    DESCRIPTION "std::condition_variable vs std::counting_semaphore producer/consumer demo"
)

# C++20 is required because std::counting_semaphore lives in <semaphore>,
# which was added in C++20. The wait-condition path would actually compile
# under C++11, but we keep one standard for the whole project.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit build/compile_commands.json. clangd reads it directly, and the
# Microsoft C/C++ extension can use it as a fallback when CMake Tools is
# not the active configuration provider. This is what makes IntelliSense
# pick up the actual compile flags (including -std=c++20).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# std::thread / std::condition_variable / std::counting_semaphore all need
# the platform's threading library at link time on most non-MSVC toolchains
# (e.g. -pthread on GCC/Clang). find_package(Threads) provides a portable
# imported target that takes care of that.
find_package(Threads REQUIRED)

add_executable(semaphore_and_wait_condition
    main.cpp
    wait_condition.cpp
    semaphore.cpp
)

target_link_libraries(semaphore_and_wait_condition
    PRIVATE
        Threads::Threads
)

# Headers live next to the sources; make sure the build can find them no
# matter where the build tree is created.
target_include_directories(semaphore_and_wait_condition
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
