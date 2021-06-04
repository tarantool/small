add_executable(small_alloc_perf.test small_alloc_perf.c)
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(LIBRT "rt")
endif ()

target_link_libraries(small_alloc_perf.test small ${LIBRT})
include_directories("${PROJECT_SOURCE_DIR}/include")
