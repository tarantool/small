#ifndef INCLUDES_TARANTOOL_SMALL_REGION_H
#define INCLUDES_TARANTOOL_SMALL_REGION_H
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
#include <stddef.h>

struct region;

typedef void (*region_on_alloc_f)(struct region *region,
			          size_t size, void *cb_arg);
typedef void (*region_on_truncate_f)(struct region *region,
			             size_t used, void *cb_arg);

#include "small_config.h"

#ifdef ENABLE_ASAN
#  include "region_asan.h"
#endif

#ifndef ENABLE_ASAN

#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rlist.h"
#include "slab_cache.h"
#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Region allocator.
 *
 * Good for allocating objects of any size, as long as
 * all of them can be freed at once. Keeps a list of
 * order-of-page-size memory blocks, thus has no external
 * fragmentation. Does have a fair bit of internal fragmentation,
 * but only if average allocation size is close to the block size.
 * Therefore is ideal for a ton of small allocations of different
 * sizes.
 *
 * Under the hood, the allocator uses a page cache of
 * mmap()-allocated pages. Pages of the page cache are never
 * released back to the operating system.
 *
 * Thread-safety
 * -------------
 * @todo, not thread safe ATM
 *
 * Errors
 * ----------------
 * The only type of failure which can occur is a failure to
 * allocate memory. alloc() calls return NULL in this case.
 */

/** A memory region.
 *
 * A memory region is a list of memory blocks.
 *
 * It's possible to allocate a chunk of any size
 * from a region.
 * It's not possible, however, to free a single allocated
 * piece, all memory must be freed at once with region_reset() or
 * region_free().
 */

struct region
{
	struct slab_cache *cache;
	struct slab_list slabs;

	/** Callback to be called on allocation. */
	region_on_alloc_f on_alloc_cb;
	/** Callback to be called on truncation. */
	region_on_truncate_f on_truncate_cb;
	/** User supplied argument passed to the callbacks. */
	void *cb_arg;
#ifndef NDEBUG
	/**
	 * The flag is used to check that there is no 2 reservations in a row.
	 * The same check that has the ASAN version.
	 */
	size_t reserved;
#endif
};

/**
 * Initialize a memory region.
 * @sa region_free().
 */
static inline void
region_create(struct region *region, struct slab_cache *cache)
{
	region->cache = cache;
	slab_list_create(&region->slabs);
	region->on_alloc_cb = NULL;
	region->on_truncate_cb = NULL;
	region->cb_arg = NULL;
#ifndef NDEBUG
	region->reserved = false;
#endif
}

/**
 * Set callbacks for the region API.
 *
 * on_alloc_cb will be called on any allocation.
 * on_truncate_cb will be called on any truncation.
 * cb_arg is passed as callbacks argument.
 *
 * For on_alloc_cb size argument is size effectively allocated. That is in
 * case of region_aligned_alloc it includes size of nessesary padding.
 *
 * For on_truncate_cb used argument equals to region_used() after truncation.
 *
 * region_used is safe to use inside the callbacks. Inside on_alloc_cb it
 * will return usage before allocation. Inside on_truncate_cb it will
 * return usage after truncation.
 */
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

/**
 * Free all allocated objects and release the allocated
 * blocks.
 */
void
region_free(struct region *region);

static inline void
region_destroy(struct region *region)
{
	return region_free(region);
}

/** Internal: a single block in a region.  */
struct rslab
{
	/*
	 * slab is a wrapper around struct slab - with a few
	 * extra members.
	 */
	struct slab slab;
	uint32_t used;
};

static inline uint32_t
rslab_sizeof()
{
	return small_align(sizeof(struct rslab), sizeof(intptr_t));
}

static inline void *
rslab_data(struct rslab *slab)
{
	return (char *) slab + rslab_sizeof();
}

static inline void *
rslab_data_end(struct rslab *slab)
{
	return (char *)rslab_data(slab) + slab->used;
}

/** How much memory is available in a given block? */
static inline uint32_t
rslab_unused(struct rslab *slab)
{
	return slab->slab.size - rslab_sizeof() - slab->used;
}

void *
region_reserve_slow(struct region *region, size_t size);

static inline void *
region_reserve(struct region *region, size_t size)
{
	void *ptr = NULL;
	assert(!region->reserved);
	if (! rlist_empty(&region->slabs.slabs)) {
		struct rslab *slab = rlist_first_entry(&region->slabs.slabs,
						       struct rslab,
						       slab.next_in_list);
		if (size <= rslab_unused(slab))
			ptr = (char *) rslab_data(slab) + slab->used;
	}
	if (ptr == NULL)
		ptr = region_reserve_slow(region, size);
#ifndef NDEBUG
	if (ptr != NULL)
		region->reserved = true;
#endif
	return ptr;
}

/** Allocate size bytes from a region. */
static inline void *
region_alloc(struct region *region, size_t size)
{
	assert(size > 0);
#ifndef NDEBUG
	region->reserved = false;
#endif
	void *ptr = region_reserve(region, size);
	if (ptr != NULL) {
		struct rslab *slab = rlist_first_entry(&region->slabs.slabs,
						       struct rslab,
						       slab.next_in_list);
		assert(size <= rslab_unused(slab));

		if (small_unlikely(region->on_alloc_cb != NULL))
			region->on_alloc_cb(region, size, region->cb_arg);
		region->slabs.stats.used += size;
		slab->used += size;
#ifndef NDEBUG
		region->reserved = false;
#endif
	}
	return ptr;
}

static inline void *
region_aligned_reserve(struct region *region, size_t size, size_t alignment)
{
	/* reserve extra to allow for alignment */
	void *ptr = region_reserve(region, size + alignment - 1);
	/* assuming NULL==0, aligned NULL still a NULL */
	return (void *)small_align((uintptr_t)ptr, alignment);
}

static inline void *
region_aligned_alloc(struct region *region, size_t size, size_t alignment)
{
	assert(size > 0);
#ifndef NDEBUG
	region->reserved = false;
#endif
	void *ptr = region_aligned_reserve(region, size, alignment);
	if (ptr != NULL) {
		struct rslab *slab = rlist_first_entry(&region->slabs.slabs,
						       struct rslab,
						       slab.next_in_list);

		/*
		 * account for optional padding before the allocated
		 * block (alignment)
		 */
		uint32_t effective_size = (uint32_t)(
			(char *)ptr - (char *)rslab_data_end(slab) + size);

		assert(effective_size <= rslab_unused(slab));

		if (small_unlikely(region->on_alloc_cb != NULL))
			region->on_alloc_cb(region, effective_size,
					    region->cb_arg);
		region->slabs.stats.used += effective_size;
		slab->used += effective_size;
#ifndef NDEBUG
		region->reserved = false;
#endif
	}
	return ptr;
}

/**
 * Mark region as empty, but keep the blocks.
 */
static inline void
region_reset(struct region *region)
{
	if (! rlist_empty(&region->slabs.slabs)) {
		struct rslab *slab = rlist_first_entry(&region->slabs.slabs,
						       struct rslab,
						       slab.next_in_list);
		region->slabs.stats.used -= slab->used;
		slab->used = 0;
	}
#ifndef NDEBUG
	region->reserved = false;
#endif
}

/** How much memory is used by this region. */
static inline size_t
region_used(struct region *region)
{
	return region->slabs.stats.used;
}

/** Return size bytes allocated last as a single chunk. */
void *
region_join(struct region *region, size_t size);

/** How much memory is held by this region. */
static inline size_t
region_total(struct region *region)
{
	return region->slabs.stats.total;
}

static inline void
region_free_after(struct region *region, size_t after)
{
	if (region_used(region) > after)
		region_free(region);
}

/** Truncate the region to the given size */
void
region_truncate(struct region *pool, size_t size);

static inline void *
region_alloc_cb(void *ctx, size_t size)
{
	return region_alloc((struct region *) ctx, size);
}

static inline void *
region_reserve_cb(void *ctx, size_t *size)
{
	struct region *region = (struct region *) ctx;
	void *ptr = region_reserve(region, *size);
	struct rslab *slab = rlist_first_entry(&region->slabs.slabs,
					       struct rslab,
					       slab.next_in_list);
	*size = rslab_unused(slab);
	return ptr;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* ifndef ENABLE_ASAN */

#if defined(__cplusplus)

struct RegionGuard {
	struct region *region;
	size_t used;

	RegionGuard(struct region *region_arg)
		: region(region_arg),
		  used(region_used(region_arg))
        {
		/* nothing */
	}

	~RegionGuard() {
		region_truncate(region, used);
	}
};

#endif /* defined(__cplusplus) */

#define region_alloc_object(region, T, size) ({					\
	*(size) = sizeof(T);							\
	(T *)region_aligned_alloc((region), sizeof(T), alignof(T));		\
})

#define region_alloc_array(region, T, count, size) ({				\
	int _tmp_ = sizeof(T) * (count);					\
	*(size) = _tmp_;							\
	(T *)region_aligned_alloc((region), _tmp_, alignof(T));			\
})

#endif /* INCLUDES_TARANTOOL_SMALL_REGION_H */
