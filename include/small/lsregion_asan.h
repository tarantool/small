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
#include <stddef.h>
#include <stdint.h>
#include <sys/uio.h>

#include "rlist.h"
#include "slab_arena.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/**
 * ASAN friendly implementation for lsregion allocator. It has same
 * interface as regular implementation but allocates every allocation using
 * malloc(). This allows to do usual ASAN checks for memory allocation.
 * See however a bit of limitation for out-of-bound access check in
 * description of small_asan_alloc.
 *
 * Allocation alignment is as requested or 1 if not specified. Additionally
 * each allocation is not aligned on next power of 2 alignment. This improves
 * unaligned memory access check.
 */
struct lsregion {
	/** List of active (not yet freed) allocations. */
	struct rlist allocations;
	/** Total size of allocated memory. */
	size_t used;
};

/** Extra data associated with each lsregion allocation. */
struct lsregion_allocation {
	/** Link for allocations list in allocator. */
	struct rlist link;
	/** Allocation size. */
	size_t size;
	/**
	 * Actually used allocation size. May be less than size, if there was no
	 * alloc() matching a reserve(), or if alloc() uses less memory.
	 */
	size_t used;
	/** Allocation id. */
	int64_t id;
	/** Allocation alignment. */
	size_t alignment;
};

static inline void
lsregion_create(struct lsregion *lsregion, struct slab_arena *arena)
{
	(void)arena;
	lsregion->used = 0;
	rlist_create(&lsregion->allocations);
}

void *
lsregion_aligned_reserve(struct lsregion *lsregion, size_t size,
			 size_t alignment);

static inline void *
lsregion_reserve(struct lsregion *lsregion, size_t size)
{
	return lsregion_aligned_reserve(lsregion, size, 1);
}

void *
lsregion_aligned_alloc(struct lsregion *lsregion, size_t size, size_t alignment,
		       int64_t id);

static inline void *
lsregion_alloc(struct lsregion *lsregion, size_t size, int64_t id)
{
	return lsregion_aligned_alloc(lsregion, size, 1, id);
}

void
lsregion_gc(struct lsregion *lsregion, int64_t min_id);

int64_t
lsregion_to_iovec(const struct lsregion *lsregion, struct iovec *iov,
		  int *iovcnt, struct lsregion_svp *svp);

static inline void
lsregion_destroy(struct lsregion *lsregion)
{
	lsregion_gc(lsregion, INT64_MAX);
}

static inline size_t
lsregion_used(const struct lsregion *lsregion)
{
	return lsregion->used;
}

static inline size_t
lsregion_total(const struct lsregion *lsregion)
{
	return lsregion->used;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
