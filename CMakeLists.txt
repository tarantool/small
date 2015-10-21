set(lib_sources
    slab_cache.c
    region.c
    mempool.c
    slab_arena.c
    small.c
    matras.c
    ibuf.c
    obuf.c)
add_library(small ${lib_sources})
