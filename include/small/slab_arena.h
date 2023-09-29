#ifndef INCLUDES_TARANTOOL_SMALL_SLAB_ARENA_H
#define INCLUDES_TARANTOOL_SMALL_SLAB_ARENA_H
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
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

enum {
	/* Smallest possible slab size. */
	SLAB_MIN_SIZE = ((size_t)USHRT_MAX) + 1,
	/** The largest allowed amount of memory of a single arena. */
	SMALL_UNLIMITED = SIZE_MAX/2 + 1
};

/**
 * Backward compatible flags to be used with slab_arena_create().
 * Initially we have been passing MAP_SHARED or MAP_PRIVATE flags
 * only, thus to continue supporting them we need to sign new API
 * with some predefined value. For this sake we reserve high bit
 * as a mark which allows us to distinguish system independent
 * SLAB_ARENA_ flags from anything else.
 *
 * Note the SLAB_ARENA_FLAG_MARK adds a second bit into the flags,
 * use IS_SLAB_ARENA_FLAG helper for testing.
 */
#define SLAB_ARENA_FLAG_MARK	(0x80000000)
#define SLAB_ARENA_FLAG(x)	((x) | SLAB_ARENA_FLAG_MARK)
#define IS_SLAB_ARENA_FLAG(f,x)	(((f) & (x)) == (x))

enum {
	/* mmap() flags */
	SLAB_ARENA_PRIVATE	= SLAB_ARENA_FLAG(1 << 0),
	SLAB_ARENA_SHARED	= SLAB_ARENA_FLAG(1 << 1),

	/* madvise() flags */
	SLAB_ARENA_DONTDUMP	= SLAB_ARENA_FLAG(1 << 2)
};

#include "small_config.h"

#ifdef ENABLE_ASAN
#  include "slab_arena_asan.h"
#endif

#ifndef ENABLE_ASAN

#include "lf_lifo.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/**
 * slab_arena -- a source of large aligned blocks of memory.
 * MT-safe.
 * Uses a lock-free LIFO to maintain a cache of used slabs.
 * Uses a lock-free quota to limit allocating memory.
 * Never returns memory to the operating system.
 */
struct slab_arena {
	/**
	 * A lock free list of cached slabs.
	 * Initially there are no cached slabs, only arena.
	 * As slabs are used and returned to arena, the cache is
	 * used to recycle them.
	 */
	struct lf_lifo cache;
	/** A preallocated arena of size = prealloc. */
	void *arena;
	/**
	 * How much memory is preallocated during initialization
	 * of slab_arena.
	 */
	size_t prealloc;
	/**
	 * How much memory in the arena has
	 * already been initialized for slabs.
	 */
	size_t used;
	/**
	 * An external quota to which we must adhere.
	 * A quota exists to set a common limit on two arenas.
	 */
	struct quota *quota;
	/*
	 * Each object returned by arena_map() has this size.
	 * The size is provided at arena initialization.
	 * It must be a power of 2 and large enough
	 * (at least 64kb, since the two lower bytes are
	 * used for ABA counter in the lock-free list).
	 * Returned pointers are always aligned by this size.
	 *
	 * It's important to keep this value moderate to
	 * limit the overhead of partially populated slabs.
	 * It is still necessary, however, to make it settable,
	 * to allow allocation of large objects.
	 * Typical value is 4Mb, which makes it possible to
	 * allocate objects of size up to ~1MB.
	 */
	uint32_t slab_size;
	/**
	 * SLAB_ARENA_ flags for mmap() and madvise() calls.
	 */
	int flags;
};

/** Initialize an arena.  */
int
slab_arena_create(struct slab_arena *arena, struct quota *quota,
		  size_t prealloc, uint32_t slab_size, int flags);

/** Destroy an arena. */
void
slab_arena_destroy(struct slab_arena *arena);

/** Get a slab. */
void *
slab_map(struct slab_arena *arena);

/** Put a slab into cache. */
void
slab_unmap(struct slab_arena *arena, void *ptr);

/** mprotect() the preallocated arena. */
void
slab_arena_mprotect(struct slab_arena *arena);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* ifndef ENABLE_ASAN */

#endif /* INCLUDES_TARANTOOL_SMALL_SLAB_ARENA_H */
