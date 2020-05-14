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

#include "lsregion.h"

void *
lsregion_aligned_reserve_slow(struct lsregion *lsregion, size_t size,
			      size_t alignment, void **unaligned)
{
	void *pos;
	struct lslab *slab;
	struct slab_arena *arena = lsregion->arena;
	size_t slab_size = arena->slab_size;

	/* If there is an existing slab then try to use it. */
	if (! rlist_empty(&lsregion->slabs.slabs)) {
		slab = rlist_last_entry(&lsregion->slabs.slabs, struct lslab,
					next_in_list);
		assert(slab != NULL);
		*unaligned = lslab_pos(slab);
		pos = (void *)small_align((size_t)*unaligned, alignment);
		if ((char *)pos + size <= (char *)lslab_end(slab))
			return pos;
	}
	/*
	 * Need to allocate more, since it can happen, that the
	 * new slab won't be aligned by the needed alignment, and
	 * after alignment its size won't be enough.
	 */
	size_t aligned_size = size + alignment - 1;
	if (aligned_size + lslab_sizeof() > slab_size) {
		/* Large allocation, use malloc() */
		slab_size = aligned_size + lslab_sizeof();
		struct quota *quota = arena->quota;
		if (quota_use(quota, slab_size) < 0)
			return NULL;
		slab = malloc(slab_size);
		if (slab == NULL) {
			quota_release(quota, slab_size);
			return NULL;
		}
		lslab_create(slab, slab_size);
		rlist_add_tail_entry(&lsregion->slabs.slabs, slab,
				     next_in_list);
		lsregion->slabs.stats.total += slab_size;
	} else if (lsregion->cached != NULL) {
		/* If there is the cached slab then use it. */
		slab = lsregion->cached;
		lsregion->cached = NULL;
		rlist_add_tail_entry(&lsregion->slabs.slabs, slab,
				     next_in_list);
	} else {
		slab = (struct lslab *) slab_map(arena);
		if (slab == NULL)
			return NULL;
		lslab_create(slab, slab_size);
		rlist_add_tail_entry(&lsregion->slabs.slabs, slab,
				     next_in_list);
		lsregion->slabs.stats.total += slab_size;
	}
	*unaligned = lslab_pos(slab);
	pos = (void *)small_align((size_t)*unaligned, alignment);
	assert((char *)pos + size <= (char *)lslab_end(slab));
	return pos;
}
