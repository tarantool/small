#ifndef INCLUDES_TARANTOOL_SMALL_SMALL_H
#define INCLUDES_TARANTOOL_SMALL_SMALL_H
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
#include <stddef.h>
#include <stdbool.h>
#include "small_config.h"

/** Information on the memory allocation. */
struct small_alloc_info {
	/**
	 * True if the object is allocated on the large slab (by malloc),
	 * false if it is allocated on the mempool.
	 */
	bool is_large;
	/**
	 * Size of the memory block that is actually allocated for the
	 * requested size.
	 */
	size_t real_size;
};

#ifdef ENABLE_ASAN
#  include "small_asan.h"
#endif

#ifndef ENABLE_ASAN

#include <stdint.h>
#include "mempool.h"
#include "slab_arena.h"
#include "lifo.h"
#include "small_class.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/**
 * Small object allocator.
 *
 * The allocator consists of a collection of mempools.
 *
 * There is one array which contained all pools.
 * The array size limits the maximum possible number of mempools.
 * All mempools are created when creating an allocator. Their sizes and
 * count are calculated depending on alloc_factor and granularity, using
 * small_class (see small_class.h for more details).
 * When requesting a memory allocation, we can find pool with the most
 * appropriate size in time O(1), using small_class.
 */

/** Basic constants of small object allocator. */
enum {
	/** How many small mempools there can be. */
	SMALL_MEMPOOL_MAX = 1024,
};

struct small_mempool_group;

/**
 * A mempool to store objects sized from objsize_min to pool->objsize.
 * Is a member of small_mempool_cache array which contains all such pools.
 * All this pools are created when creating an allocator. Their sizes and
 * count are calculated depending on alloc_factor and granularity, using
 * small_class.
 */
struct small_mempool {
	/** the pool itself. */
	struct mempool pool;
	/**
	 * Objects starting from this size and up to
	 * pool->objsize are stored in this factored
	 * pool.
	 */
	size_t objsize_min;
	/** Small mempool group that this pool belongs to. */
	struct small_mempool_group *group;
	/**
	 * Currently used pool for memory allocation. In case waste is
	 * less than @waste_max of corresponding mempool_group, @used_pool
	 * points to this structure itself.
	 */
	struct small_mempool *used_pool;
	/**
	 * Mask of appropriate pools. It is calculated once pool is created.
	 * Values of mask for:
	 * Pool 0: 0x0001 (0000 0000 0000 0001)
	 * Pool 1: 0x0003 (0000 0000 0000 0011)
	 * Pool 2: 0x0007 (0000 0000 0000 0111)
	 * And so forth.
	 */
	uint32_t appropriate_pool_mask;
	/**
	 * Currently memory waste for a given mempool. Waste is calculated as
	 * amount of excess memory spent for storing small object in pools
	 * with large object size. For instance, if we store object with size
	 * of 15 bytes in a 64-byte pool having inactive 32-byte pool, the loss
	 * will be: 64 bytes - 32 bytes = 32 bytes.
	 */
	size_t waste;
};

struct small_mempool_group {
	/** The first pool in the group. */
	struct small_mempool *first;
	/** The last pool in the group. */
	struct small_mempool *last;
	/**
	 * Raised bit on position n means that the pool with index n can be
	 * used for allocations. At the start only one pool (the last one)
	 * is available. Also note that once pool become active, it can't
	 * become
	 */
	uint32_t active_pool_mask;
	/**
	 * Pre-calculated waste threshold reaching which small_mempool becomes
	 * activated. It is equal to slab_order_size / 4.
	 */
	size_t waste_max;
};

/** A slab allocator for a wide range of object sizes. */
struct small_alloc {
	struct slab_cache *cache;
	/** Array of all small mempools of a given allocator */
	struct small_mempool small_mempool_cache[SMALL_MEMPOOL_MAX];
	/* small_mempool_cache array real size */
	uint32_t small_mempool_cache_size;
	/** Array of all small mempool groups of a given allocator */
	struct small_mempool_group small_mempool_groups[SMALL_MEMPOOL_MAX];
	/*
	 * small_mempool_groups array real size. In the worst case each
	 * group will contain only one pool, so the number of groups is
	 * also limited by SMALL_MEMPOOL_MAX.
	 */
	uint32_t small_mempool_groups_size;
	/**
	 * The factor used for factored pools. Must be > 1.
	 * Is provided during initialization.
	 */
	float factor;
	/** Small class for this allocator */
	struct small_class small_class;
	uint32_t objsize_max;
};

/**
 * Initialize a small memory allocator.
 * @param alloc - instance to create.
 * @param cache - pointer to used slab cache.
 * @param objsize_min - minimal object size.
 * @param granularity - alignment of objects in pools
 * @param alloc_factor - desired factor of growth object size.
 * Must be in (1, 2] range.
 * @param actual_alloc_factor real allocation factor calculated the basis of
 *        desired alloc_factor
 */
void
small_alloc_create(struct small_alloc *alloc, struct slab_cache *cache,
		   uint32_t objsize_min, unsigned granularity,
		   float alloc_factor, float *actual_alloc_factor);

/** Destroy the allocator and all allocated memory. */
void
small_alloc_destroy(struct small_alloc *alloc);

/** Allocate a piece of memory in the small allocator.
 *
 * @retval NULL   the requested size is beyond objsize_max
 *                or out of memory
 */
void *
smalloc(struct small_alloc *alloc, size_t size);

/** Free memory chunk allocated by the small allocator. */
/**
 * Free a small objects.
 *
 * This boils down to finding the object's mempool and delegating
 * to mempool_free().
 */
void
smfree(struct small_alloc *alloc, void *ptr, size_t size);

void
small_stats(struct small_alloc *alloc,
	    struct small_stats *totals,
	    int (*cb)(const void *, void *), void *cb_ctx);

static inline void
small_alloc_check(struct small_alloc *alloc)
{
	return slab_cache_check(alloc->cache);
}

/**
 * Fill `info' with the information about allocation `ptr' of size `size'.
 * See `struct small_alloc_info' for the description of each field.
 * Note that this function can return different `info->real_size' for the same
 * input, depending on the current `small_mempool->used_pool'.
 */
void
small_alloc_info(struct small_alloc *alloc, void *ptr, size_t size,
		 struct small_alloc_info *info);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* ifndef ENABLE_ASAN */

#endif /* INCLUDES_TARANTOOL_SMALL_SMALL_H */
