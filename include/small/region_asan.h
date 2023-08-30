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
#include "slab_cache.h"
#include "rlist.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

enum {
	/** Minimum size when reserving memory. See struct region comment. */
	SMALL_REGION_MIN_RESERVE = 128,
};

/**
 * ASAN friendly implementation for region allocator. It has same
 * interface as regular implementation but allocates every allocation using
 * malloc(). This allows to do usual ASAN checks for memory allocation.
 * See however a bit of limitation for out-of-bound access check in
 * description of small_asan_alloc.
 *
 * Allocation alignment is as requested or 1 if not specified. Additionally
 * each allocation is not aligned on next power of 2 alignment. This improves
 * unaligned memory access check.
 *
 * Regular implementation reserves memory in blocks (slabs which size grows
 * exponentially). We try to imitate this by reserving at least
 * SMALL_REGION_MIN_RESERVE bytes so that runtime code path with ASAN
 * implementation is similar to regular one. The overreserve is not very large
 * comparing to regular one to reduce memory waste.
 */
struct region {
	/** List of active (not yet freed) allocations. */
	struct rlist allocations;
	/** Total size of allocated memory. */
	size_t used;
	/**
	 * If not 0 then amount of memory reserved with one of
	 * the reserve methods.
	 */
	size_t reserved;

	/* These ones are the same as in the regular implementation */
	region_on_alloc_f on_alloc_cb;
	region_on_truncate_f on_truncate_cb;
	void *cb_arg;
};

/** Extra data associated with each region allocation. */
struct region_allocation {
	/** Link for allocations list in allocator. */
	struct rlist link;
	/**
	 * Number of bytes used from this allocation. It is 0 for allocation
	 * done thru one the reserve methods which is not yet allocated using
	 * allocation methods. It is less than size if size of allocation is
	 * less than size of prior reserve.
	 */
	size_t used;
	/** Allocation alignment. */
	size_t alignment;
};

static inline void
region_create(struct region *region, struct slab_cache *cache)
{
	(void)cache;
	region->used = 0;
	region->reserved = 0;
	region->on_alloc_cb = NULL;
	region->on_truncate_cb = NULL;
	region->cb_arg = NULL;
	rlist_create(&region->allocations);

}

static inline void
region_set_callbacks(struct region *region,
		     region_on_alloc_f on_alloc_cb,
		     region_on_truncate_f on_truncate_cb,
		     void *cb_arg)
{
	region->on_alloc_cb = on_alloc_cb;
	region->on_truncate_cb = on_truncate_cb;
	region->cb_arg = cb_arg;
}

void *
region_aligned_reserve(struct region *region, size_t size, size_t alignment);

static inline void *
region_reserve(struct region *region, size_t size)
{
	return region_aligned_reserve(region, size, 1);
}

void *
region_aligned_alloc(struct region *region, size_t size, size_t alignment);

static inline void *
region_alloc(struct region *region, size_t size)
{
	return region_aligned_alloc(region, size, 1);
}

void
region_truncate(struct region *region, size_t used);

static inline void
region_free(struct region *region)
{
	region_truncate(region, 0);
}

static inline void
region_destroy(struct region *region)
{
	region_free(region);
}

static inline void
region_reset(struct region *region)
{
	region_truncate(region, 0);
}

static inline size_t
region_used(struct region *region)
{
	return region->used;
}

static inline size_t
region_total(struct region *region)
{
	return region_used(region);
}

void *
region_join(struct region *region, size_t size);

static inline void *
region_alloc_cb(void *ctx, size_t size)
{
	return region_alloc((struct region *)ctx, size);
}

static inline void *
region_reserve_cb(void *ctx, size_t *size)
{
	struct region *region = (struct region *)ctx;
	void *ptr = region_reserve(region, *size);
	*size = region->reserved;
	return ptr;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
