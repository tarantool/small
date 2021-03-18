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
#include "mempool.h"
#include <stdlib.h>
#include <string.h>
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

#include "slab_cache.h"

/* slab fragmentation must reach 1/8 before it's recycled */
enum { MAX_COLD_FRACTION_LB = 3 };

static inline int
mslab_cmp(const struct mslab *lhs, const struct mslab *rhs)
{
	/* pointer arithmetics may overflow int * range. */
	return lhs > rhs ? 1 : (lhs < rhs ? -1 : 0);
}

rb_proto(, mslab_tree_, mslab_tree_t, struct mslab)

rb_gen(, mslab_tree_, mslab_tree_t, struct mslab, next_in_hot, mslab_cmp)

static inline void
mslab_create(struct mslab *slab, struct mempool *pool)
{
	slab->nfree = pool->objcount;
	slab->free_offset = pool->offset;
	slab->free_list = NULL;
	slab->in_hot_slabs = false;
	rlist_create(&slab->next_in_cold);
}

void *
mslab_alloc(struct mempool *pool, struct mslab *slab)
{
	assert(slab->nfree);
	void *result;
	if (slab->free_list) {
		/* Recycle an object from the garbage pool. */
		result = slab->free_list;
		/*
		 * In case when pool objsize is not aligned sizeof(intptr_t)
		 * boundary we can't use *(void **)slab->free_list construction,
		 * because (void **)slab->free_list has not necessary aligment.
		 * memcpy can work with misaligned address.
		 */
		memcpy(&slab->free_list, (void **)slab->free_list,
		       sizeof(void *));
	} else {
		/* Use an object from the "untouched" area of the slab. */
		result = (char *)slab + slab->free_offset;
		slab->free_offset += pool->objsize;
	}

	/* If the slab is full, remove it from the rb tree. */
	if (--slab->nfree == 0) {
		if (slab == pool->first_hot_slab) {
			pool->first_hot_slab = mslab_tree_next(&pool->hot_slabs,
								slab);
		}
		mslab_tree_remove(&pool->hot_slabs, slab);
		slab->in_hot_slabs = false;
	}
	return result;
}

void
mslab_free(struct mempool *pool, struct mslab *slab, void *ptr)
{
	/*
	 * Put object to garbage list.
	 * In case when pool objsize is not aligned sizeof(intptr_t) boundary
	 * we can't use *(void **)ptr = slab->free_list construction,
	 * because ptr has not necessary aligment. memcpy can work
	 * with misaligned address.
	 */
	memcpy((void **)ptr, &slab->free_list, sizeof(void *));
	slab->free_list = ptr;
	VALGRIND_FREELIKE_BLOCK(ptr, 0);
	VALGRIND_MAKE_MEM_DEFINED(ptr, sizeof(void *));

	slab->nfree++;

	if (slab->in_hot_slabs == false &&
	    slab->nfree >= (pool->objcount >> MAX_COLD_FRACTION_LB)) {
		/**
		 * Add this slab to the rbtree which contains
		 * sufficiently fragmented slabs.
		 */
		rlist_del_entry(slab, next_in_cold);
		mslab_tree_insert(&pool->hot_slabs, slab);
		slab->in_hot_slabs = true;
		/*
		 * Update first_hot_slab pointer if the newly
		 * added tree node is the leftmost.
		 */
		if (pool->first_hot_slab == NULL ||
		    mslab_cmp(pool->first_hot_slab, slab) == 1) {

			pool->first_hot_slab = slab;
		}
	} else if (slab->nfree == 1) {
		rlist_add_entry(&pool->cold_slabs, slab, next_in_cold);
	} else if (slab->nfree == pool->objcount) {
		/** Free the slab. */
		if (slab == pool->first_hot_slab) {
			pool->first_hot_slab =
				mslab_tree_next(&pool->hot_slabs, slab);
		}
		mslab_tree_remove(&pool->hot_slabs, slab);
		slab->in_hot_slabs = false;
		if (pool->spare > slab) {
			slab_list_del(&pool->slabs, &pool->spare->slab,
				      next_in_list);
			slab_put_with_order(pool->cache, &pool->spare->slab);
			pool->spare = slab;
		 } else if (pool->spare) {
			 slab_list_del(&pool->slabs, &slab->slab,
				       next_in_list);
			 slab_put_with_order(pool->cache, &slab->slab);
		 } else {
			 pool->spare = slab;
		 }
	}
}

void
mempool_create_with_order(struct mempool *pool, struct slab_cache *cache,
			  uint32_t objsize, uint8_t order)
{
	assert(order <= cache->order_max);
	pool->cache = cache;
	slab_list_create(&pool->slabs);
	mslab_tree_new(&pool->hot_slabs);
	pool->first_hot_slab = NULL;
	rlist_create(&pool->cold_slabs);
	pool->spare = NULL;
	pool->objsize = objsize;
	pool->slab_order = order;
	/* Total size of slab */
	uint32_t slab_size = slab_order_size(pool->cache, pool->slab_order);
	/* Calculate how many objects will actually fit in a slab. */
	pool->objcount = (slab_size - mslab_sizeof()) / objsize;
	assert(pool->objcount);
	pool->offset = slab_size - pool->objcount * pool->objsize;
	pool->slab_ptr_mask = ~(slab_order_size(cache, order) - 1);
}

void
mempool_destroy(struct mempool *pool)
{
	struct slab *slab, *tmp;
	rlist_foreach_entry_safe(slab, &pool->slabs.slabs,
				 next_in_list, tmp)
		slab_put_with_order(pool->cache, slab);
}

void *
mempool_alloc(struct mempool *pool)
{
	struct mslab *slab = pool->first_hot_slab;
	if (slab == NULL) {
		if (pool->spare) {
			slab = pool->spare;
			pool->spare = NULL;

		} else if ((slab = (struct mslab *)
			    slab_get_with_order(pool->cache,
						pool->slab_order))) {
			mslab_create(slab, pool);
			slab_list_add(&pool->slabs, &slab->slab, next_in_list);
		} else if (! rlist_empty(&pool->cold_slabs)) {
			slab = rlist_shift_entry(&pool->cold_slabs, struct mslab,
						 next_in_cold);
		} else {
			return NULL;
		}
		assert(slab->in_hot_slabs == false);
		mslab_tree_insert(&pool->hot_slabs, slab);
		slab->in_hot_slabs = true;
		pool->first_hot_slab = slab;
	}
	pool->slabs.stats.used += pool->objsize;
	void *ptr = mslab_alloc(pool, slab);
	assert(ptr != NULL);
	VALGRIND_MALLOCLIKE_BLOCK(ptr, pool->objsize, 0, 0);
	return ptr;
}

void
mempool_stats(struct mempool *pool, struct mempool_stats *stats)
{
	/* Object size. */
	stats->objsize = pool->objsize;
	/* Number of objects. */
	stats->objcount = mempool_count(pool);
	/* Size of the slab. */
	stats->slabsize = slab_order_size(pool->cache, pool->slab_order);
	/* The number of slabs. */
	stats->slabcount = pool->slabs.stats.total/stats->slabsize;
	/* How much memory is used for slabs. */
	stats->totals.used = pool->slabs.stats.used;
	/*
	 * How much memory is available. Subtract the slab size,
	 * which is allocation overhead and is not available
	 * memory.
	 */
	stats->totals.total = pool->slabs.stats.total -
		mslab_sizeof() * stats->slabcount;
}
