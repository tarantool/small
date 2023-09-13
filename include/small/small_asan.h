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
#include "rlist.h"
#include "slab_list.h"
#include "mempool.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

struct quota_lessor;

/**
 * ASAN friendly implementation for small object allocator. It has same
 * interface as regular implementation but allocates every allocation using
 * malloc(). This allows to do usual ASAN checks for memory allocation.
 * See however a bit of limitation for out-of-bound access check in
 * description of small_asan_alloc.
 *
 * Each allocation is aligned on SMALL_ASAN_ALIGNMENT. This allows to do
 * unaligned memory access check with smallest allowed granularity of allocator.
 * Additionally each allocation is not aligned on next power of 2 alignment.
 * This improves unaligned memory access check.
 *
 * Also we check that user provides same size in smfree which was in
 * smalloc call.
 *
 * Allocator also checks for quota usage however its not precisely the same
 * as in the regular implementation.
 *
 * Stats are limited in particular because this implementation does not
 * have same inner structure as regular one (does not consist of mempools).
 */
struct small_alloc {
	/** Quota. */
	struct quota_lessor *quota;
	/** Number of active (not yet freed) allocations. */
	size_t objcount;
	/** Total size of allocations. */
	size_t used;
	/** Unique id among all allocators. */
	unsigned int id;
};

enum {
	/** Allocations alignment. */
	SMALL_ASAN_ALIGNMENT = 4,
};

/** Extra data associated with each small object allocation. */
struct small_object {
	/** Size of allocation. */
	size_t size;
	/** id of the allocator that this object belongs to. */
	unsigned int allocator_id;
};

struct slab_cache;

void
small_alloc_create(struct small_alloc *alloc, struct slab_cache *cache,
		   uint32_t objsize_min, unsigned granularity,
		   float alloc_factor, float *actual_alloc_factor);

static inline void
small_alloc_destroy(struct small_alloc *alloc)
{
	(void)alloc;
}

void *
smalloc(struct small_alloc *alloc, size_t size);

void
smfree(struct small_alloc *alloc, void *ptr, size_t size);

void
small_stats(struct small_alloc *alloc,
	    struct small_stats *totals,
	    int (*cb)(const void *, void *), void *cb_ctx);

static inline void
small_alloc_check(struct small_alloc *alloc)
{
	(void)alloc;
	return;
}

static inline void
small_alloc_info(struct small_alloc *alloc, void *ptr, size_t size,
		 struct small_alloc_info *info)
{
	(void)alloc;
	(void)ptr;
	info->is_large = true;
	info->real_size = size;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
