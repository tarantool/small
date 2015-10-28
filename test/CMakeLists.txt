# Fix compilation by C++
add_definitions("-D__STDC_FORMAT_MACROS=1")
add_definitions("-D__STDC_LIMIT_MACROS=1")
add_definitions("-D__STDC_CONSTANT_MACROS=1")

add_executable(slab_cache.test slab_cache.c)
target_link_libraries(slab_cache.test small)
target_include_directories(slab_cache.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(region.test region.c)
target_link_libraries(region.test small)
target_include_directories(region.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(ibuf.test ibuf.c unit.c)
target_link_libraries(ibuf.test small)
target_include_directories(ibuf.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(obuf.test obuf.c)
target_link_libraries(obuf.test small)
target_include_directories(obuf.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(mempool.test mempool.c)
target_link_libraries(mempool.test small)
target_include_directories(mempool.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(small_alloc.test small_alloc.c)
target_link_libraries(small_alloc.test small)
target_include_directories(small_alloc.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(lf_lifo.test lf_lifo.c)
target_include_directories(lf_lifo.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(slab_arena.test slab_arena.c)
target_link_libraries(slab_arena.test small)
target_include_directories(slab_arena.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(arena_mt.test arena_mt.c unit.c)
target_link_libraries(arena_mt.test small pthread)
target_include_directories(arena_mt.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(matras.test matras.cc)
target_link_libraries(matras.test small)
target_include_directories(matras.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)
add_executable(quota.test quota.cc unit.c)
target_link_libraries(quota.test pthread)
target_include_directories(quota.test PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)

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
