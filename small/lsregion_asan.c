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
#include "lsregion.h"

static SMALL_NO_SANITIZE_ADDRESS struct lsregion_allocation *
lsregion_reserved_alloc(struct lsregion *lsregion)
{
	if (!rlist_empty(&lsregion->allocations)) {
		struct lsregion_allocation *alloc = NULL;
		alloc = rlist_last_entry(&lsregion->allocations, typeof(*alloc),
					 link);
		if (alloc->id == LSLAB_NOT_USED_ID)
			return alloc;
	}
	return NULL;
}

static SMALL_NO_SANITIZE_ADDRESS struct lsregion_allocation *
lsregion_prepare_buf(struct lsregion *lsregion, size_t size, size_t alignment)
{
	struct lsregion_allocation *alloc = small_asan_alloc(size, alignment,
							     sizeof(*alloc));
	alloc->size = size;
	alloc->used = 0;
	alloc->id = LSLAB_NOT_USED_ID;
	alloc->alignment = alignment;
	rlist_add_tail_entry(&lsregion->allocations, alloc, link);
	return alloc;
}

void *
lsregion_aligned_reserve(struct lsregion *lsregion, size_t size,
			 size_t alignment)
{
	/* Double reserve is prohibited. */
	small_asan_assert(lsregion_reserved_alloc(lsregion) == NULL);
	struct lsregion_allocation *alloc = lsregion_prepare_buf(lsregion, size,
								 alignment);
	return small_asan_payload_from_header(alloc);
}

SMALL_NO_SANITIZE_ADDRESS void *
lsregion_aligned_alloc(struct lsregion *lsregion, size_t size, size_t alignment,
		       int64_t id)
{
	struct lsregion_allocation *alloc = lsregion_reserved_alloc(lsregion);
	if (alloc == NULL)
		alloc = lsregion_prepare_buf(lsregion, size, alignment);
	small_asan_assert(alloc->size >= size && alloc->alignment == alignment);
	alloc->used = size;
	alloc->id = id;
	lsregion->used += size;
	char *payload = small_asan_payload_from_header(alloc);
	ASAN_POISON_MEMORY_REGION(payload + size, alloc->size - size);

	return small_asan_payload_from_header(alloc);
}

SMALL_NO_SANITIZE_ADDRESS void
lsregion_gc(struct lsregion *lsregion, int64_t min_id)
{
	struct lsregion_allocation *alloc, *tmp;
	rlist_foreach_entry_safe(alloc, &lsregion->allocations, link, tmp) {
		if (alloc->id > min_id)
			break;

		small_asan_assert(lsregion->used >= alloc->used);
		lsregion->used -= alloc->used;
		rlist_del_entry(alloc, link);
		small_asan_free(alloc);
	}
}

SMALL_NO_SANITIZE_ADDRESS int64_t
lsregion_to_iovec(const struct lsregion *lsregion, struct iovec *iov,
		  int *iovcnt, struct lsregion_svp *svp)
{
	struct lsregion_allocation *alloc;
	int64_t prev_id = svp->slab_id;
	int max_iovcnt = *iovcnt;
	int64_t max_alloc_id = LSLAB_NOT_USED_ID;
	int cnt = 0;
	rlist_foreach_entry(alloc, &lsregion->allocations, link) {
		/* An already seen allocation. */
		if (alloc->id <= prev_id)
			continue;
		if (cnt >= max_iovcnt)
			break;
		iov->iov_base = small_asan_payload_from_header(alloc);
		iov->iov_len = alloc->used;
		svp->slab_id = alloc->id;
		/*
		 * This isn't correct but will do for tests which span one slab
		 * in a normal lsregion implementation. pos isn't used to
		 * determine flush position here anyway, so it's ok.
		 */
		svp->pos += alloc->size;
		max_alloc_id = alloc->id;

		iov++;
		cnt++;
	}
	*iovcnt = cnt;
	return max_alloc_id;
}
