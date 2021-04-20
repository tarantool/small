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

enum {
	POOL_PER_GROUP_MAX = 32,
};

/**
 * Updates info in pool group for all pools with
 * object size less than or equal this pool object size.
 * First of all mempools allocates memory from the greatest
 * mempool in group. Then when waste for some mempool in
 * group becames greater or equal then waste_max, we mark this
 * mempool as available for allocation for all mempools in group,
 * with object size less than or equal this pool object size,
 * using this function. Also in case when memory for some mempool,
 * which is not greatest in group, released and waste for this pool
 * is equal to zero, we mark this pool as not available for allocation.
 * @param[in] small_mempool - 1. if available flag is true, this is pool whose
 *                            waste has become greater or equal equal then
 *                            waste_max. Now for all objects whose size are
 *                            in the range from small_mempool->objsize_min to
 *                            small_mempool->pool.objsize the memory will be
 *                            allocated from this small_mempool. Also all pools
 *                            in group with smaller object size and waste less
 *                            then waste_max will allocate memory from this
 *                            pool.
 *                            2. if available flag is false, this is pool, which
 *                            memory is totally released and waste == 0. We mark
 *                            this pool as not available for allocation.
 * @param[in] available - flag, indicates is this pool available/not available
 *                        for allocation
 */
static inline void
small_mempool_update_group(struct small_mempool *small_mempool, bool available)
{
	/*
	 * Calculate small_mempool index, and update used pool mask for group
	 */
	uint32_t idx = small_mempool - small_mempool->group->first;
	if (available)
		small_mempool->group->used_pool_mask |= (UINT32_C(1) << idx);
	else
		small_mempool->group->used_pool_mask &= ~((UINT32_C(1) << idx));
	/*
	 * We update info for all pools in group.
	 */
	for (struct small_mempool *pool = small_mempool->group->first;
	     pool <= small_mempool->group->last; pool++) {
		/*
		 * Recalculate pools for allocation.
		 * We select the pool with the lowest index,
		 * thus reducing allocation waste.
		 */
		int used_idx =
			__builtin_ffs(small_mempool->group->used_pool_mask &
				      pool->appropriate_pool_mask) - 1;
		assert(used_idx >= 0);
		pool->used_pool = small_mempool->group->first + used_idx;
		assert(pool->used_pool >= pool);
	}
}

/**
 * Checks that this pool memory is totaly released, this
 * pool is not last in group and waste for this pool == 0
 * (There are no objects that was allocated in a larger pool,
 * but in fact this pool is optimal for them). If all this
 * conditions are true, marks this pool as not available for
 * allocations and frees it's spare slab.
 * @param[in] small_mempool - small mempool.
 */
static inline void
small_mempool_check_and_free_spare(struct small_mempool *small_mempool)
{
	/*
	 * In case when pool memory is totally released,
	 * this pool is not last in group and there are no
	 * memory waste for this pool (because of allocation
	 * from pools with greater objects size), we mark
	 * this pool as not available for allocation and
	 * released it's mempool spare slab.
	 */
	if (small_mempool->pool.spare != NULL &&
	    mempool_count(&small_mempool->pool) == 0 &&
	    small_mempool != small_mempool->group->last &&
	    small_mempool->waste == 0) {
		mempool_free_spare_slab(&small_mempool->pool);
		small_mempool_update_group(small_mempool, false);
	}
}

/**
 * Calculates last pool in group.
 * In a group of pools with the same pool size,
 * there can be no more than 32 pools
 * @param[in] first - first pool in new group.
 * @param[in] last - last pool with same slab_size.
 * @return last small pool in group.
 */
static inline struct small_mempool*
calculate_last_pool_in_group(struct small_mempool *first,
			     struct small_mempool *last)
{
	uint32_t c = 0;
	struct small_mempool *last_in_group = first;
	while (last_in_group < last && (c++) < POOL_PER_GROUP_MAX - 1)
		last_in_group++;
	return last_in_group;
}

static inline void
calculate_appropriate_pool_mask(struct small_mempool *small_mempool)
{
	small_mempool->appropriate_pool_mask = 0;
	unsigned idx = small_mempool - small_mempool->group->first;
	for (unsigned i = idx; i < POOL_PER_GROUP_MAX; i++)
		small_mempool->appropriate_pool_mask |= (UINT32_C(1) << i);
}

/**
 * Creates new small pool group. First assigns all the pools
 * in the group their indexes, then mark last pool in group as
 * available for allocation. From now all mempools in group will
 * allocate memory from it, as long as their waste < waste_max.
 * @param[in] alloc - small allocator.
 * @param[in] first -first pool in group.
 * @param[in] last - last pool in group.
 */
static inline void
small_mempool_create_group(struct small_alloc *alloc,
			   struct small_mempool *first,
			   struct small_mempool *last)
{
	struct small_mempool_group *group =
		&alloc->small_mempool_groups[alloc->small_mempool_groups_size];
	group->first = first;
	group->last = last;
	group->used_pool_mask = 0;
	group->waste_max = slab_order_size(last->pool.cache,
					   last->pool.slab_order) / 4;
	while (first <= last) {
		first->group = group;
		calculate_appropriate_pool_mask(first);
		first++;
	}
	++alloc->small_mempool_groups_size;
	small_mempool_update_group(last, true);
}

/**
 * Creates one or more groups of pools from pools with the same slab size.
 * (One group for 32 or less pools).
 * @param[in] alloc - small allocator.
 * @param[in] first - First pool with same slab size.
 * @param[in] last - Last pool with same slab size.
 */
static inline void
small_mempool_create_groups(struct small_alloc *alloc,
			    struct small_mempool *first,
			    struct small_mempool *last)
{
	struct small_mempool *first_in_group = first;
	while (first_in_group <= last) {
		struct small_mempool *last_in_group =
			calculate_last_pool_in_group(first_in_group, last);
		small_mempool_create_group(alloc, first_in_group,
					   last_in_group);
		first_in_group = last_in_group + 1;
	}
}

static inline struct small_mempool *
small_mempool_search(struct small_alloc *alloc, size_t size)
{
	if (size > alloc->objsize_max)
		return NULL;
	unsigned cls =
		small_class_calc_offset_by_size(&alloc->small_class, size);
	struct small_mempool *pool = &alloc->small_mempool_cache[cls];
	return pool;
}

static inline void
small_mempool_create(struct small_alloc *alloc)
{
	uint32_t slab_order_cur = 0, slab_order_next = 0;
	size_t objsize = 0;
	struct small_mempool *cur_order_pool = &alloc->small_mempool_cache[0];
	alloc->small_mempool_groups_size = 0;

	for (alloc->small_mempool_cache_size = 0;
	     objsize < alloc->objsize_max &&
	     alloc->small_mempool_cache_size < SMALL_MEMPOOL_MAX;
	     alloc->small_mempool_cache_size++) {
		size_t prevsize = objsize;
		uint32_t mempool_cache_size = alloc->small_mempool_cache_size;
		objsize = small_class_calc_size_by_offset(&alloc->small_class,
							  mempool_cache_size);
		if (objsize > alloc->objsize_max)
			objsize = alloc->objsize_max;
		struct small_mempool *pool =
			&alloc->small_mempool_cache[mempool_cache_size];
		mempool_create(&pool->pool, alloc->cache, objsize);
		pool->pool.small_mempool = pool;
		pool->objsize_min = prevsize + 1;
		pool->group = NULL;
		pool->used_pool = NULL;
		pool->appropriate_pool_mask = 0;
		pool->waste = 0;

		slab_order_cur = (slab_order_cur == 0 ?
				  pool->pool.slab_order : slab_order_cur);
		slab_order_next = pool->pool.slab_order;
		/*
		 * In the case when the size of slab changes, create one or
		 * more mempool groups. The count of groups depends on the
		 * mempools count with same slab size. There can be no more
		 * than 32 pools in one group.
		 */
		if (slab_order_next != slab_order_cur) {
			slab_order_cur = slab_order_next;
			small_mempool_create_groups(alloc, cur_order_pool,
						    pool - 1);
			cur_order_pool = pool;
		}
		/*
		 * Maximum object size for mempool allocation ==
		 * alloc->objsize_max. If we have reached this size,
		 * there will be no more pools - loop will be breaked
		 * at the next iteration. So we need to create last
		 * group of pools.
		 */
		if (objsize == alloc->objsize_max) {
			small_mempool_create_groups(alloc, cur_order_pool,
						    pool);
		}
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
	small_mempool_create(alloc);

	lifo_init(&alloc->delayed);
	lifo_init(&alloc->delayed_large);
	alloc->free_mode = SMALL_FREE;
}

void
small_alloc_setopt(struct small_alloc *alloc, enum small_opt opt, bool val)
{
	switch (opt) {
	case SMALL_DELAYED_FREE_MODE:
		alloc->free_mode = val ? SMALL_DELAYED_FREE :
			SMALL_COLLECT_GARBAGE;
		break;
	default:
		assert(false);
		break;
	}
}

static inline void
small_collect_garbage(struct small_alloc *alloc)
{
	if (alloc->free_mode != SMALL_COLLECT_GARBAGE)
		return;

	const int BATCH = 100;
	if (!lifo_is_empty(&alloc->delayed_large)) {
		/* Free large allocations */
		for (int i = 0; i < BATCH; i++) {
			void *item = lifo_pop(&alloc->delayed_large);
			if (item == NULL)
				break;
			struct slab *slab = slab_from_data(item);
			slab_put_large(alloc->cache, slab);
		}
	} else if (!lifo_is_empty(&alloc->delayed)) {
		/* Free regular allocations */
		struct mempool *pool = lifo_peek(&alloc->delayed);
		for (int i = 0; i < BATCH; i++) {
			void *item = lifo_pop(&pool->delayed);
			if (item == NULL) {
				(void) lifo_pop(&alloc->delayed);
				pool = lifo_peek(&alloc->delayed);
				if (pool == NULL)
					break;
				continue;
			}

			/*
			 * Find mempool from which the memory was actually
			 * allocated and recalculate waste.
			 */
			struct mslab *slab = (struct mslab *)
				slab_from_ptr(item, pool->slab_ptr_mask);
			pool->small_mempool->waste -=
				(slab->mempool->objsize -
				 pool->objsize);
			mempool_free_slab(slab->mempool, slab, item);
		}
	} else {
		/* Finish garbage collection and switch to regular mode */
		alloc->free_mode = SMALL_FREE;
	}
}


/**
 * Allocate a small object.
 *
 * Find a mempool instance of the right size, using
 * small_class, and allocate the object on the pool.
 *
 * @retval ptr success
 * @retval NULL out of memory
 */
void *
smalloc(struct small_alloc *alloc, size_t size)
{
	small_collect_garbage(alloc);

	struct small_mempool *small_mempool = small_mempool_search(alloc, size);
	if (small_mempool == NULL) {
		/* Object is too large, fallback to slab_cache */
		struct slab *slab = slab_get_large(alloc->cache, size);
		if (slab == NULL)
			return NULL;
		return slab_data(slab);
	}
	struct mempool *pool = &small_mempool->used_pool->pool;
	assert(size <= pool->objsize);
	void *ptr = mempool_alloc(pool);
	if (ptr != NULL)
		goto success;

	for (unsigned i = 0; i < alloc->small_mempool_cache_size; i++) {
		struct small_mempool *tmp =
			&alloc->small_mempool_cache[i];
		small_mempool_check_and_free_spare(tmp);
	}

	ptr = mempool_alloc(pool);

success:
	if (ptr != NULL && small_mempool->used_pool != small_mempool) {
		/*
		 * Waste for this allocation is the difference between
		 * the size of objects optimum mempool and used mempool
		 */
		small_mempool->waste +=
			(small_mempool->used_pool->pool.objsize -
			 small_mempool->pool.objsize);
		/*
		 * In case when waste for this mempool became greater then
		 * or equal to waste_max, we are updating the information
		 * for the mempool group that this mempool belongs to,
		 * that it can now be used for memory allocation.
		 */
		if (small_mempool->waste >= small_mempool->group->waste_max)
			small_mempool_update_group(small_mempool, true);
	}
	return ptr;
}

static inline struct mempool *
mempool_find(struct small_alloc *alloc, size_t size)
{
	struct small_mempool *small_mempool = small_mempool_search(alloc, size);
	if (small_mempool == NULL)
		return NULL; /* Allocated by slab_cache. */
	assert(size >= small_mempool->objsize_min);
	struct mempool *pool = &small_mempool->pool;
	assert(size <= pool->objsize);
	return pool;
}

/** Free memory chunk allocated by the small allocator. */
/**
 * Free a small object.
 *
 * This boils down to finding the object's mempool and delegating
 * to mempool_free().
 */
void
smfree(struct small_alloc *alloc, void *ptr, size_t size)
{
	struct small_mempool *pool = small_mempool_search(alloc, size);
	if (pool == NULL) {
		/* Large allocation by slab_cache */
		struct slab *slab = slab_from_data(ptr);
		slab_put_large(alloc->cache, slab);
		return;
	}

	struct mslab *slab = (struct mslab *)
		slab_from_ptr(ptr, pool->pool.slab_ptr_mask);
	/*
	 * In case this ptr was allocated from other small mempool
	 * reducing waste for current pool (as you remember, waste
	 * in our case is memory loss due to allocation from large pools).
	 */
	pool->waste -= (slab->mempool->objsize - pool->pool.objsize);
	/* Regular allocation in mempools */
	mempool_free_slab(slab->mempool, slab, ptr);
}

/**
 * Free memory chunk allocated by the small allocator
 * if not in snapshot mode, otherwise put to the delayed
 * free list.
 */
void
smfree_delayed(struct small_alloc *alloc, void *ptr, size_t size)
{
	if (alloc->free_mode == SMALL_DELAYED_FREE && ptr) {
		struct mempool *pool = mempool_find(alloc, size);
		if (pool == NULL) {
			/* Large-object allocation by slab_cache. */
			lifo_push(&alloc->delayed_large, ptr);
			return;
		}
		/* Regular allocation in mempools */
		if (lifo_is_empty(&pool->delayed))
			lifo_push(&alloc->delayed, &pool->link);
		lifo_push(&pool->delayed, ptr);
	} else {
		smfree(alloc, ptr, size);
	}
}

/** Simplify iteration over small allocator mempools. */
struct mempool_iterator
{
	struct small_alloc *alloc;
	uint32_t small_iterator;
};

void
mempool_iterator_create(struct mempool_iterator *it,
			struct small_alloc *alloc)
{
	it->alloc = alloc;
	it->small_iterator = 0;
}

struct mempool *
mempool_iterator_next(struct mempool_iterator *it)
{
	struct small_mempool *small_mempool = NULL;
	if (it->small_iterator < it->alloc->small_mempool_cache_size)
		small_mempool =
			&it->alloc->small_mempool_cache[(it->small_iterator)++];
	if (small_mempool)
		return &(small_mempool->pool);

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
	lifo_init(&alloc->delayed);

	/* Free large allocations */
	void *item;
	while ((item = lifo_pop(&alloc->delayed_large))) {
		struct slab *slab = slab_from_data(item);
		slab_put_large(alloc->cache, slab);
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
