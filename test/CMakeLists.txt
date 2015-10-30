# Fix compilation by C++
add_definitions("-D__STDC_FORMAT_MACROS=1")
add_definitions("-D__STDC_LIMIT_MACROS=1")
add_definitions("-D__STDC_CONSTANT_MACROS=1")

add_executable(slab_cache.test slab_cache.c)
target_link_libraries(slab_cache.test small)

add_executable(region.test region.c)
target_link_libraries(region.test small)

add_executable(ibuf.test ibuf.c unit.c)
target_link_libraries(ibuf.test small)

add_executable(obuf.test obuf.c)
target_link_libraries(obuf.test small)

add_executable(mempool.test mempool.c)
target_link_libraries(mempool.test small)

add_executable(small_alloc.test small_alloc.c)
target_link_libraries(small_alloc.test small)

add_executable(lf_lifo.test lf_lifo.c)

add_executable(slab_arena.test slab_arena.c)
target_link_libraries(slab_arena.test small)

add_executable(arena_mt.test arena_mt.c unit.c)
target_link_libraries(arena_mt.test small pthread)

add_executable(matras.test matras.cc)
target_link_libraries(matras.test small)

add_executable(quota.test quota.cc unit.c)
target_link_libraries(quota.test pthread)

include_directories("${PROJECT_SOURCE_DIR}")

if(POLICY CMP0037)
    cmake_policy(SET CMP0037 OLD) # don't blame "test" target name
endif(POLICY CMP0037)

add_custom_target(test
    WORKING_DIRECTORY "${CURRENT_BINARY_DIR}"
    COMMAND ./test-run.sh
    DEPENDS slab_cache.test region.test ibuf.test obuf.test mempool.test
            small_alloc.test lf_lifo.test slab_arena.test arena_mt.test
            matras.test quota.test
    )
