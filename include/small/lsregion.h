#ifndef INCLUDES_TARANTOOL_LSREGION_H
#define INCLUDES_TARANTOOL_LSREGION_H
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
#include <assert.h>
#include <stdbool.h>

#include "rlist.h"
#include "quota.h"
#include "slab_cache.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#define LSLAB_NOT_USED_ID -1

/**
 * Wrapper for a slab that tracks a size of used memory and
 * maximal identifier of memory that was allocated in the slab.
 */
struct lslab {
	/** Link in the lsregion.slabs list. */
	struct rlist next_in_list;
	/**
	 * Slab allocated size.
	 */
	size_t slab_size;
	/**
	 * Size of used memory including aligned size of this
	 * structure.
	 */
	size_t slab_used;
	/**
	 * Maximal id that was used to alloc data from the slab.
	 */
	int64_t max_id;
};

/**
 * Log structured allocator treats memory as a sequentially
 * written log. It allows to allocate memory chunks of any size,
 * but does not support free() of an individual chunk. Instead,
 * each chunk, when allocated, needs to be identified with an id.
 * It is assumed that ids are nondecreasing.
 * The chunks are stored in equally-sized slabs, obtained from
 * slab arena.
 * To free memory, the allocator requires an oldest id before
 * which all memory could be discarded. Upon free, it returns
 * all slabs containing chunks with smaller ids to the slab arena.
 *
 * id_i <= id_(i + 1)
 * *-------*   *-------*   *-------*                  *-------*
 * | slab  |-->| slab  |-->| slab  |-->            -->| slab  |
 * *-------*   *-------*   *-------*                  *-------*
 *  <= id1      <= id2 |    <= id3                     <= idN
 *                     |
 * truncate with id in [id2, id3) deletes from this position.
 */
struct lsregion {
	/**
	 * List of memory slabs and the statistics. The older a
	 * slab is, the closer it is placed to front of the list.
	 */
	struct slab_list slabs;
	/** Slabs arena - source for memory slabs. */
	struct slab_arena *arena;
	struct lslab *cached;
};

/** Aligned size of the struct lslab. */
static inline size_t
lslab_sizeof()
{
	return small_align(sizeof(struct lslab), sizeof(intptr_t));
}

/** Initialize the lslab object. */
static inline void
lslab_create(struct lslab *slab, size_t size)
{
	rlist_create(&slab->next_in_list);
	slab->slab_size = size;
	slab->slab_used = lslab_sizeof();
	slab->max_id = LSLAB_NOT_USED_ID;
}

/**
 * Size of the unused part of the slab.
 * @param slab     Slab container.
 * @retval Unsed memory size.
 */
static inline size_t
lslab_unused(const struct lslab *slab)
{
	assert(slab->slab_size >= slab->slab_used);
	return slab->slab_size - slab->slab_used;
}

/**
 * Update slab statistics and meta according to new allocation.
 */
static inline void
lslab_use(struct lslab *slab, size_t size, int64_t id)
{
	assert(size <= lslab_unused(slab));
	assert(slab->max_id <= id);
	slab->slab_used += size;
	slab->max_id = id;
}

/**
 * Pointer to the end of the used part of the slab.
 * @param slab Slab container.
 * @retval Pointer to the unused part of the slab.
 */
static inline void *
lslab_pos(struct lslab *slab)
{
	return (char *) slab + slab->slab_used;
}

/** Pointer to the end of the slab memory. */
static inline void *
lslab_end(struct lslab *slab)
{
	return (char *)slab + slab->slab_size;
}

/**
 * Initialize log structured allocator.
 * @param lsregion Allocator object.
 * @param arena    Slabs arena.
 */
static inline void
lsregion_create(struct lsregion *lsregion, struct slab_arena *arena)
{
	assert(arena != NULL);
	assert(arena->slab_size > lslab_sizeof());
	slab_list_create(&lsregion->slabs);
	lsregion->arena = arena;
	lsregion->cached = NULL;
}

/** @sa lsregion_aligned_reserve(). */
void *
lsregion_aligned_reserve_slow(struct lsregion *lsregion, size_t size,
			      size_t alignment, void **unaligned);

/**
 * Make sure a next allocation of at least @a size bytes will not
 * fail, and will return the same result as this call, aligned by
 * @a alignment.
 * @param lsregion Allocator object.
 * @param size     Size to allocate.
 * @param alignment Byte alignment required for the result
 *        address.
 * @param[out] unaligned When the function succeeds, it returns
 *        aligned address, and stores base unaligned address to
 *        this variable. That helps to find how many bytes were
 *        wasted to make the alignment.
 *
 * @retval not NULL Success.
 * @retval NULL     Memory error.
 */
static inline void *
lsregion_aligned_reserve(struct lsregion *lsregion, size_t size,
			 size_t alignment, void **unaligned)
{
	/* If there is an existing slab then try to use it. */
	if (! rlist_empty(&lsregion->slabs.slabs)) {
		struct lslab *slab;
		slab = rlist_last_entry(&lsregion->slabs.slabs, struct lslab,
					next_in_list);
		assert(slab != NULL);
		*unaligned = lslab_pos(slab);
		void *pos = (void *)small_align((size_t)*unaligned, alignment);
		if ((char *)pos + size <= (char *)lslab_end(slab))
			return pos;
	}
	return lsregion_aligned_reserve_slow(lsregion, size, alignment,
					     unaligned);
}

/**
 * Make sure a next allocation of at least @a size bytes will not
 * fail, and will return the same result as this call.
 * @param lsregion Allocator object.
 * @param size Size to allocate.
 *
 * @retval not-NULL Success.
 * @retval NULL Memory error.
 */
static inline void *
lsregion_reserve(struct lsregion *lsregion, size_t size)
{
	void *unaligned = NULL;
	void *res = lsregion_aligned_reserve(lsregion, size, 1, &unaligned);
	assert(res == NULL || res == unaligned);
	return res;
}

/**
 * Allocate @a size bytes and associate the allocated block
 * with @a id.
 * @param lsregion Allocator object.
 * @param size Size to allocate.
 * @param id Memory chunk identifier.
 *
 * @retval not-NULL Success.
 * @retval NULL Memory error.
 */
static inline void *
lsregion_alloc(struct lsregion *lsregion, size_t size, int64_t id)
{
	void *res = lsregion_reserve(lsregion, size);
	if (res == NULL)
		return NULL;
	struct lslab *slab = rlist_last_entry(&lsregion->slabs.slabs,
					      struct lslab, next_in_list);
	lslab_use(slab, size, id);
	lsregion->slabs.stats.used += size;
	return res;
}

/**
 * The same as normal alloc, but the resulting pointer is aligned
 * by @a alignment.
 */
static inline void *
lsregion_aligned_alloc(struct lsregion *lsregion, size_t size, size_t alignment,
		       int64_t id)
{
	void *unaligned;
	void *res = lsregion_aligned_reserve(lsregion, size, alignment,
					     &unaligned);
	if (res == NULL)
		return NULL;
	struct lslab *slab = rlist_last_entry(&lsregion->slabs.slabs,
					      struct lslab, next_in_list);
	size += (char *)res - (char *)unaligned;
	lslab_use(slab, size, id);
	lsregion->slabs.stats.used += size;
	return res;
}

#define lsregion_alloc_object(lsregion, id, T)					\
	(T *)lsregion_aligned_alloc((lsregion), sizeof(T), alignof(T), (id))

/**
 * Try to free all memory blocks in which the biggest identifier
 * is less or equal then the specified identifier.
 * @param lsregion Allocator object.
 * @param min_id   Free all memory blocks with
 *                 max_id <= this parameter.
 */
static inline void
lsregion_gc(struct lsregion *lsregion, int64_t min_id)
{
	struct lslab *slab, *next;
	size_t arena_slab_size = lsregion->arena->slab_size;
	/*
	 * First blocks are the oldest so free them until
	 * max_id > min_id.
	 */
	rlist_foreach_entry_safe(slab, &lsregion->slabs.slabs, next_in_list,
				 next) {
		if (slab->max_id > min_id)
			break;
		rlist_del_entry(slab, next_in_list);
		/*
		 * lslab_sizeof() must not affect the used bytes
		 * count.
		 */
		lsregion->slabs.stats.used -= slab->slab_used - lslab_sizeof();
		if (slab->slab_size > arena_slab_size) {
			/* Never put large slabs into cache */
			quota_release(lsregion->arena->quota, slab->slab_size);
			lsregion->slabs.stats.total -= slab->slab_size;
			free(slab);
		} else if (lsregion->cached != NULL) {
			lsregion->slabs.stats.total -= slab->slab_size;
			slab_unmap(lsregion->arena, slab);
		} else {
			lslab_create(slab, slab->slab_size);
			lsregion->cached = slab;
		}
	}
}

/**
 * Free all resources occupied by the allocator.
 * @param lsregion Allocator object.
 */
static inline void
lsregion_destroy(struct lsregion *lsregion)
{
	if (! rlist_empty(&lsregion->slabs.slabs))
		lsregion_gc(lsregion, INT64_MAX);
	if (lsregion->cached != NULL)
		slab_unmap(lsregion->arena, lsregion->cached);
}

/** Size of the allocated memory. */
static inline size_t
lsregion_used(const struct lsregion *lsregion)
{
	return lsregion->slabs.stats.used;
}

/** Size of the allocated and reserved memory. */
static inline size_t
lsregion_total(const struct lsregion *lsregion)
{
	return lsregion->slabs.stats.total;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif
