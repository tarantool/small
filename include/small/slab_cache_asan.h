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
#include "quota_lessor.h"
#include "util.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

enum {
	/** Alignment for slabs from ASAN slab cache. */
	SMALL_CACHE_SLAB_ALIGNMENT = sizeof(long long),
};

/**
 * ASAN friendly implementation for slab cache allocator. It has same
 * interface as regular implementation but allocates every allocation using
 * malloc(). This allows to do usual ASAN checks for memory allocation.
 *
 * This implementation do not follow no regular implementation slab order
 * nor slab alignment.
 */
struct slab_cache {
	/**
	 * Allocation quota. It is not used directly. We need to keep quota
	 * to pass it to small object allocator only.
	 */
	struct quota_lessor quota;
	size_t used;
	/** Unique id among all allocators. */
	unsigned int id;
	pthread_t thread_id;
};

/** Header of slab cache allocation. */
struct slab {
	/** Allocation size. */
	size_t size;
};

/** Extra data associated with each slab. */
struct slab_cache_object {
	/** id of the cache that this object belongs to. */
	unsigned int cache_id;
};

void
slab_cache_create(struct slab_cache *cache, struct slab_arena *arena);

static inline void
slab_cache_destroy(struct slab_cache *cache)
{
	quota_lessor_destroy(&cache->quota);
}

static inline size_t
slab_real_size(struct slab_cache *cache, size_t size)
{
	(void)cache;
	/* Copy-paste of slab_sizeof() */
	size_t meta_size = small_align(sizeof(struct slab), sizeof(intptr_t));
	return small_align(size + meta_size, small_getpagesize());
}

struct slab *
slab_get(struct slab_cache *cache, size_t size);

void
slab_put(struct slab_cache *cache, struct slab *slab);

static inline size_t
slab_cache_used(struct slab_cache *cache)
{
	return cache->used;
}

static inline void
slab_cache_check(struct slab_cache *cache)
{
	(void)cache;
}

static inline void
slab_cache_set_thread(struct slab_cache *cache)
{
	(void)cache;
	cache->thread_id = pthread_self();
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
