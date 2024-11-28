set(CMAKE_CXX_STANDARD 11)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(AUTHOR_WARNING "Benchmark available only in release build")
    return()
endif()

# Handle a case when BENCHMARK_LIBRARIES, BENCHMARK_INCLUDE_DIRS
# and benchmark_FOUND are defined in the parent project, like in
# the Tarantool, that uses the bundled Benchmark library.
if (NOT benchmark_FOUND)
    find_package(benchmark QUIET)
    if (NOT benchmark_FOUND)
        message(AUTHOR_WARNING "Google Benchmark library not found")
        return()
    endif()
    set(BENCHMARK_LIBRARIES benchmark::benchmark)
endif()

include_directories("${PROJECT_SOURCE_DIR}")

add_executable(small.perftest small.cc)
target_link_libraries(small.perftest small ${BENCHMARK_LIBRARIES} pthread)
target_include_directories(small.perftest PUBLIC ${BENCHMARK_INCLUDE_DIRS})
