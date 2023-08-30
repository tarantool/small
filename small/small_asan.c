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
#include "small.h"
#include "quota_lessor.h"
#include "mempool.h"
#include "slab_cache.h"

void
small_alloc_create(struct small_alloc *alloc, struct slab_cache *cache,
		   uint32_t objsize_min, unsigned granularity,
		   float alloc_factor, float *actual_alloc_factor)
{
	(void)objsize_min;
	(void)granularity;
	(void)alloc_factor;
	(void)cache;
	rlist_create(&alloc->objects);
	alloc->used = 0;
	alloc->objcount = 0;
	*actual_alloc_factor = alloc_factor;
}

SMALL_NO_SANITIZE_ADDRESS void
small_alloc_destroy(struct small_alloc *alloc)
{
	struct small_object *obj, *tmp;
	rlist_foreach_entry_safe(obj, &alloc->objects, link, tmp) {
		small_asan_free(obj);
	}
	rlist_create(&alloc->objects);
}

SMALL_NO_SANITIZE_ADDRESS void *
smalloc(struct small_alloc *alloc, size_t size)
{
	struct small_object *obj =
				small_asan_alloc(size, SMALL_ASAN_ALIGNMENT,
						 sizeof(struct small_object));
	obj->size = size;
	rlist_add_entry(&alloc->objects, obj, link);
	alloc->used += size;
	alloc->objcount++;

	return small_asan_payload_from_header(obj);
}

SMALL_NO_SANITIZE_ADDRESS void
smfree(struct small_alloc *alloc, void *ptr, size_t size)
{
	struct small_object *obj = small_asan_header_from_payload(ptr);
	small_asan_assert(obj->size == size && "invalid object size");
	rlist_del_entry(obj, link);
	alloc->used -= obj->size;
	alloc->objcount--;

	small_asan_free(obj);
}

void
small_stats(struct small_alloc *alloc,
	    struct small_stats *totals,
	    int (*cb)(const void *, void *), void *cb_ctx)
{
	(void)alloc;
	(void)cb;
	(void)cb_ctx;
	totals->used = alloc->used;
	totals->total = alloc->used;

	/* At least we can report total object count */
	struct mempool_stats stats;
	memset(&stats, 0, sizeof(stats));
	stats.objcount = alloc->objcount;
	cb(&stats, cb_ctx);
}
