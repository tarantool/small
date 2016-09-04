project(small C CXX)
cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(WIN32)
    set(CMAKE_USE_RELATIVE_PATHS 1)
endif()

# if(NOT DEFINED SMALL_EMBEDDED)
    if (WIN32)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wall")
        # set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od /WX")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /Od")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wall")
        # set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /WX")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od")
    else()
        set(CC_NATIVE_OPTS -march=native)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra ${CC_NATIVE_OPTS}")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -Werror")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra ${CC_NATIVE_OPTS}")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Werror")
    endif()
# endif()

# Enable GNU glibc extentions.
add_definitions("-D_GNU_SOURCE")

set(lib_headers
    small/builtins.h
    small/ibuf.h
    small/lf_lifo.h
    small/lifo.h
    small/matras.h
    small/mempool.h
    small/obuf.h
    small/pm_mmap.h
    small/pmatomic.h
    small/pm_pthread.h
    small/quota.h
    small/rb.h
    small/region.h
    small/rlist.h
    small/slab_arena.h
    small/slab_cache.h
    small/small.h
    small/win32/pmvcatomic.h
    small/win32/vcunistd.h
    small/win32/xatomic.h
    small/win32/xatomic0.h)

set(lib_sources
    small/slab_cache.c
    small/region.c
    small/mempool.c
    small/slab_arena.c
    small/small.c
    small/matras.c
    small/ibuf.c
    small/obuf.c)

if (WIN32)
    set(lib_sources ${lib_sources} 
                    small/pm_mmap.c
                    small/pm_pthread.c)
endif()

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
