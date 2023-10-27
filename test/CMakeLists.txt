# Fix compilation by C++
add_definitions("-D__STDC_FORMAT_MACROS=1")
add_definitions("-D__STDC_LIMIT_MACROS=1")
add_definitions("-D__STDC_CONSTANT_MACROS=1")

add_library(small_unit STATIC unit.c)

add_executable(slab_cache.test slab_cache.c)
target_link_libraries(slab_cache.test small small_unit)

add_executable(region.test region.c)
target_link_libraries(region.test small small_unit)

add_executable(ibuf.test ibuf.c)
target_link_libraries(ibuf.test small small_unit)

add_executable(obuf.test obuf.c)
target_link_libraries(obuf.test small small_unit)

add_executable(rlist.test rlist.c)
target_link_libraries(rlist.test small small_unit)

add_executable(rb.test rb.c)
target_link_libraries(rb.test small small_unit)

add_executable(rb_aug.test rb_aug.c)
target_link_libraries(rb_aug.test small small_unit)

add_executable(rb_rand.test rb_rand.cc)
set_source_files_properties(rb_rand.cc PROPERTIES
    COMPILE_FLAGS "-std=gnu++0x")
target_link_libraries(rb_rand.test small_unit)

add_executable(mempool.test mempool.c)
target_link_libraries(mempool.test small small_unit)

add_executable(small_class.test small_class.c
               ${PROJECT_SOURCE_DIR}/small/small_class.c)
target_link_libraries(small_class.test small small_unit)

add_executable(small_class_branchless.test
               small_class.c
               ${PROJECT_SOURCE_DIR}/small/small_class.c)
target_link_libraries(small_class_branchless.test small small_unit)
target_compile_definitions(small_class_branchless.test PUBLIC SMALL_CLASS_BRANCHLESS)

set(small_sources
    small_alloc.c
    ${PROJECT_SOURCE_DIR}/small/util.c)

# ASAN implementation has different source files.
if(NOT ENABLE_ASAN)
    list(APPEND small_sources
         ${PROJECT_SOURCE_DIR}/small/mempool.c
         ${PROJECT_SOURCE_DIR}/small/slab_arena.c
         ${PROJECT_SOURCE_DIR}/small/slab_cache.c
         ${PROJECT_SOURCE_DIR}/small/small.c
         ${PROJECT_SOURCE_DIR}/small/small_class.c)
else()
    list(APPEND small_sources
         ${PROJECT_SOURCE_DIR}/small/slab_arena_asan.c
         ${PROJECT_SOURCE_DIR}/small/slab_cache_asan.c
         ${PROJECT_SOURCE_DIR}/small/small_asan.c)
endif()

set(small_alloc_tests "")
function(build_small_alloc_test SLAB_MIN_ORDER0_SIZE)
    set(target_name "small_alloc_${SLAB_MIN_ORDER0_SIZE}.test")
    add_executable(${target_name} ${small_sources})
    target_compile_definitions(
        ${target_name} PUBLIC SLAB_MIN_ORDER0_SIZE=${SLAB_MIN_ORDER0_SIZE}
    )
    target_link_libraries(${target_name} m small_unit)
    list(APPEND small_alloc_tests ${target_name})
    add_test(${target_name} ${CMAKE_CURRENT_BINARY_DIR}/${target_name})
endfunction()

foreach(SLAB_MIN_ORDER0_SIZE 4096 8192 16384 32768 65536)
    build_small_alloc_test(${SLAB_MIN_ORDER0_SIZE})
endforeach()

add_executable(lf_lifo.test lf_lifo.c)
target_link_libraries(lf_lifo.test small small_unit)

add_executable(slab_arena.test slab_arena.c)
target_link_libraries(slab_arena.test small small_unit)

add_executable(arena_mt.test arena_mt.c)
target_link_libraries(arena_mt.test small pthread small_unit)

add_executable(matras.test matras.cc)
target_link_libraries(matras.test small small_unit)

add_executable(lsregion.test lsregion.c)
target_link_libraries(lsregion.test small small_unit)

add_executable(quota.test quota.cc)
target_link_libraries(quota.test pthread small_unit)

add_executable(quota_lessor.test quota_lessor.c)
target_link_libraries(quota_lessor.test pthread small_unit)

add_executable(static.test static.c)
target_link_libraries(static.test pthread small small_unit)

add_executable(util.test util.c)
target_link_libraries(util.test small small_unit)

# Granularity is not supported in ASAN implementation.
if(NOT ENABLE_ASAN)
    add_executable(small_granularity.test small_granularity.c)
    target_link_libraries(small_granularity.test small small_unit)
endif()

include_directories("${PROJECT_SOURCE_DIR}/include")

add_test(slab_cache ${CMAKE_CURRENT_BINARY_DIR}/slab_cache.test)
add_test(region ${CMAKE_CURRENT_BINARY_DIR}/region.test)
add_test(ibuf ${CMAKE_CURRENT_BINARY_DIR}/ibuf.test)
add_test(obuf ${CMAKE_CURRENT_BINARY_DIR}/obuf.test)
add_test(mempool ${CMAKE_CURRENT_BINARY_DIR}/mempool.test)
add_test(small_class ${CMAKE_CURRENT_BINARY_DIR}/small_class.test)
add_test(small_class_branchless ${CMAKE_CURRENT_BINARY_DIR}/small_class_branchless.test)
add_test(lf_lifo ${CMAKE_CURRENT_BINARY_DIR}/lf_lifo.test)
add_test(arena_mt ${CMAKE_CURRENT_BINARY_DIR}/arena_mt.test)
add_test(matras ${CMAKE_CURRENT_BINARY_DIR}/matras.test)
add_test(lsregion ${CMAKE_CURRENT_BINARY_DIR}/lsregion.test)
add_test(quota ${CMAKE_CURRENT_BINARY_DIR}/quota.test)
add_test(quota_lessor ${CMAKE_CURRENT_BINARY_DIR}/quota_lessor.test)
add_test(rb ${CMAKE_CURRENT_BINARY_DIR}/rb.test)
add_test(rb_aug ${CMAKE_CURRENT_BINARY_DIR}/rb_aug.test)
add_test(rb_rand ${CMAKE_CURRENT_BINARY_DIR}/rb_rand.test)
add_test(static ${CMAKE_CURRENT_BINARY_DIR}/static.test)
add_test(rlist ${CMAKE_CURRENT_BINARY_DIR}/rlist.test)
add_test(util ${CMAKE_CURRENT_BINARY_DIR}/util.test)

if(NOT ENABLE_ASAN)
    add_test(small_granularity ${CMAKE_CURRENT_BINARY_DIR}/small_granularity.test)
endif()

if(DEFINED SMALL_EMBEDDED)
    return()
endif()

if(POLICY CMP0037)
    cmake_policy(SET CMP0037 OLD) # don't blame "test" target name
endif(POLICY CMP0037)

set(small_tests
    slab_cache.test
    region.test
    ibuf.test
    obuf.test
    mempool.test
    ${small_alloc_tests}
    lf_lifo.test
    slab_arena.test
    arena_mt.test
    matras.test
    lsregion.test
    quota.test
    util.test
    rb.test)

if(NOT ENABLE_ASAN)
    list(APPEND small_tests
         small_granularity.test)
endif()

add_custom_target(test
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    COMMAND ctest
    DEPENDS ${small_tests}
)
