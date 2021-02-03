# peformance test for small allocator

To build the test, you need to install google benchmark.
You can get detailed instructions here https://github.com/google/benchmark.
To compare performance, it is useful to use compare.py utility from google
benchmark https://github.com/google/benchmark/blob/master/docs/tools.md. But
be careful, if the test failed with an error, the utility will assume that the
execution time of the operation under test is zero.
Typical usage examples:
1. ./small.perftest - run all performance tests
2. ./small.perftest --benchmark_filter=<regex> - run all performance tests, with
                                                 a name that partially matches regex
3. ./compare.py benchmarks <old> <new> - run and compare two benchmarks.
