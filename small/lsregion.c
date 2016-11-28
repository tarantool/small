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

struct lslab *
lsregion_reserve_lslab_slow(struct lsregion *lsregion, size_t size, int64_t id)
{
	(void) id; /* Used only for debug. */
	assert(lsregion != NULL);
	assert(size > 0);
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
		/* For details @sa lsregion_reserve_lslab(). */
		if (slab->max_id == LSLAB_NOT_USED_ID) {
			struct lslab *prev;
			prev = rlist_prev_entry_safe(slab,
						     &lsregion->slabs.slabs,
						     next_in_list);
			if (prev != NULL &&
			    lslab_unused(lsregion, prev) >= size)
				slab = prev;
		}
	}

	/*
	 * In case of an empty lsregion or the full last slab we
	 * need to reserve a new one.
	 */
	if (slab == NULL || size > lslab_unused(lsregion, slab)) {
		/*
		 * Reserve memory for data and for the header of
		 * the lslab wrapper.
		 */
		slab = (struct lslab *) slab_map(lsregion->arena);
		if (slab == NULL)
			return NULL;
		lslab_create(slab);
		rlist_add_tail_entry(&lsregion->slabs.slabs, slab,
				     next_in_list);
		lsregion->slabs.stats.total += lsregion->slab_size;
	}
	assert(slab != NULL);
	return slab;
}
