#ifndef INCLUDES_TARANTOOL_SMALL_SLAB_CACHE_H
#define INCLUDES_TARANTOOL_SMALL_SLAB_CACHE_H
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
#include "slab_arena.h"
#include <pthread.h>

#include "small_config.h"

#ifdef ENABLE_ASAN
#  include "slab_cache_asan.h"
#endif

#ifndef ENABLE_ASAN

#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <assert.h>
#include "rlist.h"
#include "slab_list.h"
#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

extern const uint32_t slab_magic;

struct slab {
	/*
	 * Next slab in the list of allocated slabs. Unused if
	 * this slab has a buddy. Sic: if a slab is not allocated
	 * but is made by a split of a larger (allocated) slab,
	 * this member got to be left intact, to not corrupt
	 * cache->allocated list.
	 */
	struct rlist next_in_cache;
	/** Next slab in slab_list->slabs list. */
	struct rlist next_in_list;
	/**
	 * Allocated size.
	 * Is different from (SLAB_MIN_SIZE << slab->order)
	 * when requested size is bigger than SLAB_MAX_SIZE
	 * (i.e. slab->order is SLAB_CLASS_LAST).
	 */
	size_t size;
	/** Slab magic (for sanity checks). */
	uint32_t magic;
	/** Base of lb(size) for ordered slabs. */
	uint8_t order;
	/**
	 * Only used for buddy slabs. If the buddy of the current
	 * free slab is also free, both slabs are merged and
	 * a free slab of the higher order emerges.
	 * Value of 0 means the slab is free. Otherwise
	 * slab->in_use is set to slab->order + 1.
	 */
	uint8_t in_use;
};

/*
 * A binary logarithmic distance between the smallest and
 * the largest slab in the cache can't be that big, really.
 */
enum { ORDER_MAX = 16 };

struct slab_cache {
	/* The source of allocations for this cache. */
	struct slab_arena *arena;
	/*
	 * Min size of the slab in the cache maintained
	 * using the buddy system. The logarithmic distance
	 * between order0_size and arena->slab_max_size
	 * defines the number of "orders" of slab cache.
	 * This distance can't be more than ORDER_MAX.
	 */
	uint32_t order0_size;
	/*
	 * Binary logarithm of order0_size, useful in pointer
	 * arithmetics.
	 */
	uint8_t order0_size_lb;
	/*
	 * Slabs of order in range [0, order_max) have size
	 * which is a power of 2. Slabs in the next order are
	 * double the size of the previous order.  Slabs of the
	 * previous order are obtained by splitting a slab of the
	 * next order, and so on until order is order_max
	 * Slabs of order order_max are obtained directly
	 * from slab_arena. This system is also known as buddy
	 * system.
	 */
	uint8_t order_max;
	/** All allocated slabs used in the cache.
	 * The stats reflect the total used/allocated
	 * memory in the cache.
	 */
	struct slab_list allocated;
	/**
	 * Lists of unused slabs, for each slab order.
	 *
	 * A used slab is removed from the list and its
	 * next_in_list link may be reused for some other purpose.
	 */
	struct slab_list orders[ORDER_MAX+1];
#ifndef NDEBUG
	pthread_t thread_id;
#endif
};

void
slab_cache_create(struct slab_cache *cache, struct slab_arena *arena);

void
slab_cache_destroy(struct slab_cache *cache);

/**
 * Allocate ordered slab
 * @see slab_order()
 */
struct slab *
slab_get_with_order(struct slab_cache *cache, uint8_t order);

/**
 * Deallocate ordered slab
 */
void
slab_put_with_order(struct slab_cache *cache, struct slab *slab);

/**
 * Allocate large slab.
 * @pre size > slab_order_size(cache->arena->slab_size)
 */
struct slab *
slab_get_large(struct slab_cache *slab, size_t size);

/**
 * Deallocate large slab.
 * @pre slab was allocated with slab_get_large()
 */
void
slab_put_large(struct slab_cache *cache, struct slab *slab);

/**
 * A shortcut for slab_get_with_order()/slab_get_large()
 * @see slab_get_with_order()
 * @see slab_get_large()
 */
struct slab *
slab_get(struct slab_cache *cache, size_t size);

/**
 * Shortcut for slab_put_with_order()/slab_put_large()
 * @see slab_get_with_order()
 * @see slab_get_large()
 */
void
slab_put(struct slab_cache *cache, struct slab *slab);

/**
 * Return the number of bytes used by this slab cache.
 * @remark This function is thread-safe.
 */
static inline size_t
slab_cache_used(struct slab_cache *slabc)
{
	return slabc->allocated.stats.used;
}

/**
 * Given a pointer allocated in a slab, get the handle
 * of the slab itself.
 */
static inline struct slab *
slab_from_ptr(void *ptr, intptr_t slab_mask)
{
	intptr_t addr = (intptr_t) ptr;
	/** All memory mapped slabs are slab->size aligned. */
	struct slab *slab = (struct slab *)(addr & slab_mask);
	assert(slab->magic == slab_magic);
	return slab;
}

void
slab_cache_check(struct slab_cache *cache);

/**
 * Find the nearest power of 2 size capable of containing
 * a chunk of the given size. Adjust for cache->order0_size
 * and arena->slab_size.
 */
static inline uint8_t
slab_order(struct slab_cache *cache, size_t size)
{
	if (size <= cache->order0_size)
		return 0;
	if (size > cache->arena->slab_size)
		return cache->order_max + 1;

	return (uint8_t) (CHAR_BIT * sizeof(unsigned) -
			  __builtin_clz((unsigned) size - 1) -
			  cache->order0_size_lb);
}

/** Convert slab order to the mmap()ed size. */
static inline intptr_t
slab_order_size(struct slab_cache *cache, uint8_t order)
{
	assert(order <= cache->order_max);
	intptr_t size = 1;
	return size << (order + cache->order0_size_lb);
}

/**
 * Given the requested size, calculate the actual size of a slab, that will be
 * allocated by slab_get(). Note that the real capacity of such a slab will be
 * less than the real size by slab_sizeof().
 */
size_t
slab_real_size(struct slab_cache *cache, size_t size);

/**
 * Debug only: track that all allocations
 * are made from a single thread.
 */
static inline void
slab_cache_set_thread(struct slab_cache *cache)
{
	(void) cache;
#ifndef NDEBUG
	cache->thread_id = pthread_self();
#endif
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* ifndef ENABLE_ASAN */

/** Aligned size of slab meta. */
static inline size_t
slab_sizeof(void)
{
	return small_align(sizeof(struct slab), sizeof(intptr_t));
}

/** Useful size of a slab. */
static inline size_t
slab_capacity(struct slab *slab)
{
	return slab->size - slab_sizeof();
}

static inline struct slab *
slab_from_data(void *data)
{
	return (struct slab *)((char *)data - slab_sizeof());
}

static inline void *
slab_data(struct slab *slab)
{
	return (char *)slab + slab_sizeof();
}

#endif /* INCLUDES_TARANTOOL_SMALL_SLAB_CACHE_H */
