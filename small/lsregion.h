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
	 * Size of used memory including aligned size of this
	 * structure.
	 */
	uint32_t slab_used;
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
	/** Copy of slab_arena.slab_size. */
	uint32_t slab_size;
	struct lslab *cached;
};

/** Aligned size of the struct lslab. */
static inline uint32_t
lslab_sizeof()
{
	return small_align(sizeof(struct lslab), sizeof(intptr_t));
}

/** Initialize the lslab object. */
static inline void
lslab_create(struct lslab *slab)
{
	rlist_create(&slab->next_in_list);
	slab->slab_used = lslab_sizeof();
	slab->max_id = LSLAB_NOT_USED_ID;
}

/**
 * Size of the unused part of the slab.
 * @param lsregion Allocator object.
 * @param slab     Slab container.
 * @retval Unsed memory size.
 */
static inline uint32_t
lslab_unused(const struct lsregion *lsregion, const struct lslab *slab)
{
	uint32_t slab_size = lsregion->slab_size;
	assert(slab_size > slab->slab_used);
	return slab_size - slab->slab_used;
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
	lsregion->slab_size = arena->slab_size;
	lsregion->cached = NULL;
}

/**
 * Find existing or allocate a new slab, that has \p size unused
 * memory space.
 * @param lsregion Allocator object.
 * @param size     Size to reserve.
 * @param id       Allocation id. It is assumed that ids are
 *                 nondecreasing.
 *
 * @retval not NULL Success.
 * @retval NULL     Memory error, or too big size.
 */
struct lslab *
lsregion_reserve_lslab_slow(struct lsregion *lsregion, size_t size, int64_t id);

static inline struct lslab *
lsregion_reserve_lslab(struct lsregion *lsregion, size_t size, int64_t id)
{
	struct lslab *slab = NULL;

	/* Can't reserve more than the arena can map. */
	if (size + lslab_sizeof() > lsregion->slab_size)
		return NULL;

	/* If there is an existing slab then try to use it. */
	if (! rlist_empty(&lsregion->slabs.slabs)) {
		slab = rlist_last_entry(&lsregion->slabs.slabs, struct lslab,
					next_in_list);
		assert(slab != NULL);
		assert(slab->max_id <= id);
		/*
		 * If the newest slab is only reserved but is not
		 * used then check if the previous slab maybe has
		 * needed bytes count.
		 *
		 * Here data still can be allocated in the slab 1.
		 * *----------*------*     *---------------------*
		 * | occupied | free | --> |         free        |
		 * *----------*------*     *---------------------*
		 *       slab 1                     slab 2
		 *
		 * Here data can be allocated only in the second
		 * or a new slab.
		 * *----------*------*     *---------------------*
		 * | occupied | free | --> | occupied |   free   |
		 * *----------*------*     *---------------------*
		 *       slab 1                     slab 2
		 */
		if (slab->max_id != LSLAB_NOT_USED_ID) {
			if (size <= lslab_unused(lsregion, slab))
				return slab;
			/* Fall to the slow version. */
			return lsregion_reserve_lslab_slow(lsregion, size, id);
		}
		struct lslab *prev;
		prev = rlist_prev_entry_safe(slab, &lsregion->slabs.slabs,
					     next_in_list);
		if (prev != NULL &&
		    size <= lslab_unused(lsregion, prev))
			return prev;
	}
	/* If there is the cached slab then use it. */
	if (lsregion->cached != NULL) {
		slab = lsregion->cached;
		lsregion->cached = NULL;
		rlist_add_tail_entry(&lsregion->slabs.slabs, slab,
				     next_in_list);
		return slab;
	}
	/* Fall back to the slow version. */
	return lsregion_reserve_lslab_slow(lsregion, size, id);
}

/**
 * Reserve \p size bytes. Guarantees, that for a next allocation
 * of bytes <= \p size a new slab will not be mapped.
 * @param lsregion Allocator object.
 * @param size     Size to reserve.
 * @param id       Memory block identifier.
 *
 * @retval not NULL Success.
 * @retval NULL     Memory error.
 */
static inline void *
lsregion_reserve(struct lsregion *lsregion, size_t size, int64_t id)
{
	struct lslab *slab = lsregion_reserve_lslab(lsregion, size, id);
	if (slab == NULL)
		return NULL;
	return lslab_pos(slab);
}

/**
 * Allocate \p size bytes and assicoate the allocated block
 * with \p id.
 * @param lsregion Allocator object.
 * @param size     Size to allocate.
 * @param id       Memory chunk identifier.
 *
 * @retval not NULL Success.
 * @retval NULL     Memory error.
 */
static inline void *
lsregion_alloc(struct lsregion *lsregion, size_t size, int64_t id)
{
	/* Reserve and occupy a memory block. */
	struct lslab *slab = lsregion_reserve_lslab(lsregion, size, id);
	if (slab == NULL)
		return NULL;
	void *res = lslab_pos(slab);
	slab->slab_used += size;

	/* Update the memory block meta info. */
	assert(slab->max_id <= id);
	slab->max_id = id;
	lsregion->slabs.stats.used += size;
	return res;
}

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
		if (lsregion->cached != NULL) {
			slab_unmap(lsregion->arena, slab);
			lsregion->slabs.stats.total -= lsregion->slab_size;
		} else {
			lslab_create(slab);
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

/** Size of the alloced memory. */
static inline uint32_t
lsregion_used(const struct lsregion *lsregion)
{
	return lsregion->slabs.stats.used;
}

/** Size of the alloced and reserved memory. */
static inline uint32_t
lsregion_total(const struct lsregion *lsregion)
{
	return lsregion->slabs.stats.total;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif
