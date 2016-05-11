project(small C CXX)
cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Werror")

# Enable GNU glibc extentions.
add_definitions("-D_GNU_SOURCE")

set(lib_headers
    small/ibuf.h
    small/lf_lifo.h
    small/lifo.h
    small/matras.h
    small/mempool.h
    small/obuf.h
    small/pmatomic.h
    small/quota.h
    small/rb.h
    small/region.h
    small/rlist.h
    small/slab_arena.h
    small/slab_cache.h
    small/small.h)

set(lib_sources
    small/slab_cache.c
    small/region.c
    small/mempool.c
    small/slab_arena.c
    small/small.c
    small/matras.c
    small/ibuf.c
    small/obuf.c)

add_library(${PROJECT_NAME} STATIC ${lib_sources})

enable_testing()
add_subdirectory(test)

if(DEFINED SMALL_EMBEDDED)
    # Don't build shared library and skip INSTALL() targets if this
    # library is used as submodule in other project.
    return()
endif()

add_library(${PROJECT_NAME}_shared SHARED ${lib_sources})
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
