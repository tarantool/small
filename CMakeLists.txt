project(small C CXX)
cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

include(CheckFunctionExists)
include(CheckSymbolExists)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT DEFINED SMALL_EMBEDDED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Werror")
endif()

# Enable GNU glibc extentions.
add_definitions("-D_GNU_SOURCE")

set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")

check_symbol_exists(MAP_ANON sys/mman.h TARANTOOL_SMALL_HAVE_MAP_ANON)
check_symbol_exists(MAP_ANONYMOUS sys/mman.h TARANTOOL_SMALL_HAVE_MAP_ANONYMOUS)

check_function_exists(madvise TARANTOOL_SMALL_HAVE_MADVISE)
check_symbol_exists(MADV_DONTDUMP sys/mman.h TARANTOOL_SMALL_HAVE_MADV_DONTDUMP)

set(config_h "${CMAKE_CURRENT_BINARY_DIR}/small/include/small_config.h")
configure_file(
    "small/small_config.h.cmake"
    "${config_h}"
    )
message (STATUS "")

include_directories("${CMAKE_CURRENT_BINARY_DIR}/small/include")
include_directories("${PROJECT_SOURCE_DIR}/include/small")

# Valgrind
include_directories(third_party)

set(lib_headers
    "${config_h}"
    include/small/util.h
    include/small/small_features.h
    include/small/ibuf.h
    include/small/lf_lifo.h
    include/small/lifo.h
    include/small/matras.h
    include/small/mempool.h
    include/small/obuf.h
    include/small/quota.h
    include/small/rb.h
    include/small/region.h
    include/small/rlist.h
    include/small/slab_arena.h
    include/small/slab_cache.h
    include/small/slab_list.h
    include/small/small_class.h
    include/small/small.h
    include/small/lsregion.h
    include/small/static.h)

# ASAN implementation has extra headers that are included from regular headers.
if(ENABLE_ASAN)
    list(APPEND lib_headers
         include/small/region_asan.h
         include/small/small_asan.h)
endif()

set(lib_sources
    small/util.c
    small/small_features.c
    small/slab_cache.c
    small/mempool.c
    small/slab_arena.c
    small/matras.c
    small/ibuf.c
    small/obuf.c
    small/lsregion.c
    small/static.c)

if(ENABLE_ASAN)
    list(APPEND lib_sources
         small/region_asan.c
         small/small_asan.c)
else()
    list(APPEND lib_sources
         small/region.c
         small/small_class.c
         small/small.c)
endif()

add_library(${PROJECT_NAME} STATIC ${lib_sources})
target_link_libraries(${PROJECT_NAME} m)

enable_testing()
add_subdirectory(test)
add_subdirectory(perf)

if(DEFINED SMALL_EMBEDDED)
    # Don't build shared library and skip INSTALL() targets if this
    # library is used as submodule in other project.
    return()
endif()

option(ENABLE_VALGRIND "Enable integration with valgrind, a memory analyzing tool" OFF)
if (NOT ENABLE_VALGRIND)
    add_definitions(-DNVALGRIND=1)
endif()

add_library(${PROJECT_NAME}_shared SHARED ${lib_sources})
target_link_libraries(${PROJECT_NAME}_shared m)
set_target_properties(${PROJECT_NAME}_shared PROPERTIES VERSION 1.0 SOVERSION 1)
set_target_properties(${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT library)

install(TARGETS ${PROJECT_NAME}_shared
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT library)

install(FILES ${lib_headers} DESTINATION include/${PROJECT_NAME})
install(DIRECTORY third_party DESTINATION include/${PROJECT_NAME} FILES_MATCHING PATTERN "*.h")
