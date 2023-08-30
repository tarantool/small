/*
 * Copyright 2010-2023, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stddef.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/**
 * ASAN friendly implementation for mempool allocator. It has same
 * interface as regular implementation but allocates every allocation using
 * malloc(). This allows to do usual ASAN checks for memory allocation.
 * See however a bit of limitation for out-of-bound access check in
 * description of small_asan_alloc.
 *
 * Allocation alignment is the same as in regular implementation.
 * Alignment is determined by the size of mempool object. If size has
 * 0 bit set then alignment is 1, if size has 1 bit set (with lower bits
 * set to 0) then alignment is 2 etc etc. If size is power of 2 then
 * alignment is same as size. This alignment follows from the fact that
 * regular mempool is just an array with last element aligned on very
 * high power of 2.
 *
 * However for ASAN implementation alignment is limited to
 * MEMPOOL_ASAN_MAX_ALIGNMENT to prevent high memory waste. Typically we
 * don't need high alignment while merely object size may force it to be high.
 *
 * Additionally each allocation is not aligned on next power of 2 alignment.
 * This improves unaligned memory access check.
 *
 * Stats are limited in particular because this implementation does not
 * have same inner structure as regular one (does not use slabs).
 */
struct mempool {
	/* Size of every allocation. */
	uint32_t objsize;
	/** Number of active (not yet freed) allocations. */
	size_t objcount;
	/** Alignment of every allocation. */
	size_t alignment;
	/** Unique id among all allocators. */
	unsigned int id;
};

/** Extra data associated with each mempool object allocation. */
struct mempool_object {
	/** id of the pool that this object belongs to. */
	unsigned int pool_id;
};

void
mempool_create(struct mempool *pool, struct slab_cache *cache,
	       uint32_t objsize);

void
mempool_destroy(struct mempool *pool);

void *
mempool_alloc(struct mempool *pool);

void
mempool_free(struct mempool *pool, void *ptr);

static inline bool
mempool_is_initialized(struct mempool *pool)
{
	return pool->objsize != 0;
}

static inline size_t
mempool_count(struct mempool *pool)
{
	return pool->objcount;
}

static inline size_t
mempool_used(struct mempool *pool)
{
	return pool->objsize * pool->objcount;
}

static inline void
mempool_stats(struct mempool *pool, struct mempool_stats *stats)
{
	stats->objsize = pool->objsize;
	stats->objcount = pool->objcount;
	stats->totals.used = pool->objsize * pool->objcount;
	stats->totals.total = stats->totals.used;
	stats->slabsize = 0;
	stats->slabcount = 0;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
