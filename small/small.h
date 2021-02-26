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

enum small_opt {
	SMALL_DELAYED_FREE_MODE
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
	/** Currently used pool for memory allocation. */
	struct small_mempool *used_pool;
	/** Mask of appropriate pools. */
	uint32_t appropriate_pool_mask;
	/**
	 * Currently memory waste for a given mempool.
	 * We only take into account memory losses for allocations
	 * from mempools with larger object size.
	 */
	size_t waste;
};

struct small_mempool_group {
	/** First pool in group. */
	struct small_mempool *first;
	/** Last pool in group. */
	struct small_mempool *last;
	/**
	 * Mask for this group, which shows all pools in the
	 * group from which there was at least one allocation.
	 */
	uint32_t used_pool_mask;
	/**
	 * Maximum memory waste for a given mempool in group
	 * caused by memory allocation from mempools with large
	 * object sizes. If waste >= waste_max we start to allocate
	 * memory from a given mempool.
	 */
	size_t waste_max;
};

/**
 * Free mode
 */
enum small_free_mode {
	/** Free objects immediately. */
	SMALL_FREE,
	/** Collect garbage after delayed free. */
	SMALL_COLLECT_GARBAGE,
	/** Postpone deletion of objects. */
	SMALL_DELAYED_FREE,
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
	/* small_mempool_groups array real size */
	uint32_t small_mempool_groups_size;
	/**
	 * List of mempool which objects to be freed if delayed free mode.
	 */
	struct lifo delayed;
	/**
	 * List of large allocations by malloc() to be freed in delayed mode.
	 */
	struct lifo delayed_large;
	/**
	 * The factor used for factored pools. Must be > 1.
	 * Is provided during initialization.
	 */
	float factor;
	/** Small class for this allocator */
	struct small_class small_class;
	uint32_t objsize_max;
	/**
	 * Free mode.
	 */
	enum small_free_mode free_mode;
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

/**
 * Enter or leave delayed mode - in delayed mode smfree_delayed()
 * doesn't free chunks but puts them into a pool.
 */
void
small_alloc_setopt(struct small_alloc *alloc, enum small_opt opt, bool val);

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

/**
 * Free memory chunk allocated by the small allocator
 * if not in snapshot mode, otherwise put to the delayed
 * free list.
 */
void
smfree_delayed(struct small_alloc *alloc, void *ptr, size_t size);

/**
 * @brief Return an unique index associated with a chunk allocated
 * by the allocator.
 *
 * This index space is more dense than the pointers space,
 * especially in the least significant bits.  This number is
 * needed because some types of box's indexes (e.g. BITSET) have
 * better performance then they operate on sequential offsets
 * (i.e. dense space) instead of memory pointers (sparse space).
 *
 * The calculation is based on SLAB number and the position of an
 * item within it. Current implementation only guarantees that
 * adjacent chunks from one SLAB will have consecutive indexes.
 * That is, if two chunks were sequentially allocated from one
 * chunk they will have sequential ids. If a second chunk was
 * allocated from another SLAB thеn the difference between indexes
 * may be more than one.
 *
 * @param ptr pointer to memory allocated in small_alloc
 * @return unique index
 */
size_t
small_ptr_compress(struct small_alloc *alloc, void *ptr);

/**
 * Perform the opposite action of small_ptr_compress().
 */
void *
small_ptr_decompress(struct small_alloc *alloc, size_t val);

typedef int (*mempool_stats_cb)(const struct mempool_stats *stats,
				void *cb_ctx);

void
small_stats(struct small_alloc *alloc,
	    struct small_stats *totals,
	    mempool_stats_cb cb, void *cb_ctx);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* INCLUDES_TARANTOOL_SMALL_SMALL_H */
