/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
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
#include "small.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static inline struct factor_pool *
factor_pool_search(struct small_alloc *alloc, size_t size)
{
	if (size > alloc->objsize_max)
		return NULL;
	unsigned cls = small_class_calc_offset_by_size(&alloc->small_class, size);
	struct factor_pool *pool = &alloc->factor_pool_cache[cls];
	return pool;
}

static inline void
factor_pool_create(struct small_alloc *alloc)
{
	size_t objsize = 0;
	for (alloc->factor_pool_cache_size = 0;
	     objsize < alloc->objsize_max && alloc->factor_pool_cache_size < FACTOR_POOL_MAX;
	     alloc->factor_pool_cache_size++) {
		size_t prevsize = objsize;
		objsize = small_class_calc_size_by_offset(&alloc->small_class,
			alloc->factor_pool_cache_size);
		if (objsize > alloc->objsize_max)
			objsize = alloc->objsize_max;
		struct factor_pool *pool =
			&alloc->factor_pool_cache[alloc->factor_pool_cache_size];
		mempool_create(&pool->pool, alloc->cache, objsize);
		pool->objsize_min = prevsize + 1;
	}
	alloc->objsize_max = objsize;
}

/** Initialize the small allocator. */
void
small_alloc_create(struct small_alloc *alloc, struct slab_cache *cache,
		   uint32_t objsize_min, unsigned granularity,
		   float alloc_factor, float *actual_alloc_factor)
{
	alloc->cache = cache;
	/* Align sizes. */
	objsize_min = small_align(objsize_min, granularity);
	/* Make sure at least 4 largest objects can fit in a slab. */
	alloc->objsize_max =
		mempool_objsize_max(slab_order_size(cache, cache->order_max));
	alloc->objsize_max = small_align(alloc->objsize_max, granularity);

	assert((granularity & (granularity - 1)) == 0);
	assert(alloc_factor > 1. && alloc_factor <= 2.);

	alloc->factor = alloc_factor;
	/*
	 * Second parameter granularity, determines alignment.
	 */
	small_class_create(&alloc->small_class, granularity,
			   alloc->factor, objsize_min, actual_alloc_factor);
	factor_pool_create(alloc);
}

/**
 * Allocate a small object.
 *
 * Find or create a mempool instance of the right size,
 * and allocate the object on the pool.
 *
 * If object is small enough to fit a stepped pool,
 * finding the right pool for it is just a matter of bit
 * shifts. Otherwise, look up a pool in the red-black
 * factored pool tree.
 *
 * @retval ptr success
 * @retval NULL out of memory
 */
void *
smalloc(struct small_alloc *alloc, size_t size)
{
	struct factor_pool *upper_bound = factor_pool_search(alloc, size);
	if (upper_bound == NULL) {
		/* Object is too large, fallback to slab_cache */
		struct slab *slab = slab_get_large(alloc->cache, size);
		if (slab == NULL)
			return NULL;
		return slab_data(slab);
	}
	struct mempool *pool = &upper_bound->pool;
	assert(size <= pool->objsize);
	return mempool_alloc(pool);
}

static inline struct mempool *
mempool_find(struct small_alloc *alloc, size_t size)
{
	struct factor_pool *upper_bound = factor_pool_search(alloc, size);
	if (upper_bound == NULL)
		return NULL; /* Allocated by slab_cache. */
	assert(size >= upper_bound->objsize_min);
	struct mempool *pool = &upper_bound->pool;
	assert(size <= pool->objsize);
	return pool;
}

/** Free memory chunk allocated by the small allocator. */
/**
 * Free a small object.
 *
 * This boils down to finding the object's mempool and delegating
 * to mempool_free().
 *
 * If the pool becomes completely empty, and it's a factored pool,
 * and the factored pool's cache is empty, put back the empty
 * factored pool into the factored pool cache.
 */
void
smfree(struct small_alloc *alloc, void *ptr, size_t size)
{
	struct mempool *pool = mempool_find(alloc, size);
	if (pool == NULL) {
		/* Large allocation by slab_cache */
		struct slab *slab = slab_from_data(ptr);
		slab_put_large(alloc->cache, slab);
		return;
	}

	/* Regular allocation in mempools */
	mempool_free(pool, ptr);
}

/** Simplify iteration over small allocator mempools. */
struct mempool_iterator
{
	struct small_alloc *alloc;
	uint32_t factor_iterator;
};

void
mempool_iterator_create(struct mempool_iterator *it,
			struct small_alloc *alloc)
{
	it->alloc = alloc;
	it->factor_iterator = 0;
}

struct mempool *
mempool_iterator_next(struct mempool_iterator *it)
{
	struct factor_pool *factor_pool = NULL;
	if (it->factor_iterator < it->alloc->factor_pool_cache_size)
		factor_pool =
			&it->alloc->factor_pool_cache[(it->factor_iterator)++];
	if (factor_pool)
		return &(factor_pool->pool);

	return NULL;
}

/** Destroy all pools. */
void
small_alloc_destroy(struct small_alloc *alloc)
{
	struct mempool_iterator it;
	mempool_iterator_create(&it, alloc);
	struct mempool *pool;
	while ((pool = mempool_iterator_next(&it))) {
		mempool_destroy(pool);
	}
}

/** Calculate allocation statistics. */
void
small_stats(struct small_alloc *alloc,
	    struct small_stats *totals,
	    mempool_stats_cb cb, void *cb_ctx)
{
	memset(totals, 0, sizeof(*totals));

	struct mempool_iterator it;
	mempool_iterator_create(&it, alloc);
	struct mempool *pool;

	while ((pool = mempool_iterator_next(&it))) {
		struct mempool_stats stats;
		mempool_stats(pool, &stats);
		totals->used += stats.totals.used;
		totals->total += stats.totals.total;
		if (cb(&stats, cb_ctx))
			break;
	}
}
