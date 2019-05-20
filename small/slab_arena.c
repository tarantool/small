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
#include "config.h"
#include "quota.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>
#include <pmatomic.h>
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

static void
madvise_checked(void *ptr, size_t size, int flags)
{
#ifdef TARANTOOL_SMALL_USE_MADVISE
	if (!ptr)
		return;

	if (!IS_SLAB_ARENA_FLAG(flags, SLAB_ARENA_DONTDUMP))
		return;

	if (madvise(ptr, size, MADV_DONTDUMP)) {
		intptr_t ignore_it;
		char buf[64];

		ignore_it = (intptr_t)strerror_r(errno, buf, sizeof(buf));
		(void)ignore_it;

		fprintf(stderr, "Error in madvise(%p, %zu, 0x%x): %s\n",
			ptr, size, MADV_DONTDUMP, buf);
	}
#else
	(void)ptr;
	(void)size;
	(void)flags;
#endif
}

static void
munmap_checked(void *addr, size_t size)
{
	if (munmap(addr, size)) {
		char buf[64];
		intptr_t ignore_it = (intptr_t)strerror_r(errno, buf,
							  sizeof(buf));
		(void)ignore_it;
		fprintf(stderr, "Error in munmap(%p, %zu): %s\n",
			addr, size, buf);
		assert(false);
	}
}

static void *
mmap_checked(size_t size, size_t align, int flags)
{
	/* The alignment must be a power of two. */
	assert((align & (align - 1)) == 0);
	/* The size must be a multiple of alignment */
	assert((size & (align - 1)) == 0);

	if (IS_SLAB_ARENA_FLAG(flags, SLAB_ARENA_PRIVATE))
		flags = MAP_PRIVATE | MAP_ANONYMOUS;
	else
		flags = MAP_SHARED | MAP_ANONYMOUS;

	/*
	 * All mappings except the first are likely to
	 * be aligned already.  Be optimistic by trying
	 * to map exactly the requested amount.
	 */
	void *map = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (map == MAP_FAILED)
		return NULL;
	if (((intptr_t) map & (align - 1)) == 0)
		return map;
	munmap_checked(map, size);

	/*
	 * mmap enough amount to be able to align
	 * the mapped address.  This can lead to virtual memory
	 * fragmentation depending on the kernels allocation
	 * strategy.
	 */
	map = mmap(NULL, size + align, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (map == MAP_FAILED)
		return NULL;

	/* Align the mapped address around slab size. */
	size_t offset = (intptr_t) map & (align - 1);

	if (offset != 0) {
		/* Unmap unaligned prefix and postfix. */
		munmap_checked(map, align - offset);
		map += align - offset;
		munmap_checked(map + size, offset);
	} else {
		/* The address is returned aligned. */
		munmap_checked(map + size, align);
	}
	return map;
}

#if 0
/** This is a way to round things up without using a built-in. */
static size_t
pow2round(size_t size)
{
	int shift = 1;
	size_t res = size - 1;
	while (res & (res + 1)) {
		res |= res >> shift;
		shift <<= 1;
	}
	return res + 1;
}
#endif

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void
slab_arena_flags_init(struct slab_arena *arena, int flags)
{
	/*
	 * Old interface for backward compatibility, MAP_
	 * flags are passed directly without SLAB_ARENA_FLAG_MARK,
	 * map them to internal ones.
	 */
	if (!(flags & SLAB_ARENA_FLAG_MARK)) {
		assert(flags & (MAP_PRIVATE | MAP_SHARED));
		if (flags == MAP_PRIVATE)
			arena->flags = SLAB_ARENA_PRIVATE;
		else
			arena->flags = SLAB_ARENA_SHARED;
		return;
	}

	assert(IS_SLAB_ARENA_FLAG(flags, SLAB_ARENA_PRIVATE) ||
	       IS_SLAB_ARENA_FLAG(flags, SLAB_ARENA_SHARED));

	arena->flags = flags;
}

int
slab_arena_create(struct slab_arena *arena, struct quota *quota,
		  size_t prealloc, uint32_t slab_size, int flags)
{
	lf_lifo_init(&arena->cache);
	VALGRIND_MAKE_MEM_DEFINED(&arena->cache, sizeof(struct lf_lifo));

	/*
	 * Round up the user supplied data - it can come in
	 * directly from the configuration file. Allow
	 * zero-size arena for testing purposes.
	 */
	arena->slab_size = small_round(MAX(slab_size, SLAB_MIN_SIZE));

	arena->quota = quota;
	/** Prealloc can not be greater than the quota */
	prealloc = MIN(prealloc, quota_total(quota));
	/** Extremely large sizes can not be aligned properly */
	prealloc = MIN(prealloc, SIZE_MAX - arena->slab_size);
	/* Align prealloc around a fixed number of slabs. */
	arena->prealloc = small_align(prealloc, arena->slab_size);

	arena->used = 0;

	slab_arena_flags_init(arena, flags);

	if (arena->prealloc) {
		arena->arena = mmap_checked(arena->prealloc,
					    arena->slab_size,
					    arena->flags);
	} else {
		arena->arena = NULL;
	}

	madvise_checked(arena->arena, arena->prealloc, arena->flags);

	return arena->prealloc && !arena->arena ? -1 : 0;
}

void
slab_arena_destroy(struct slab_arena *arena)
{
	void *ptr;
	size_t total = 0;
	while ((ptr = lf_lifo_pop(&arena->cache))) {
		if (arena->arena == NULL || ptr < arena->arena ||
		    ptr >= arena->arena + arena->prealloc) {
			munmap_checked(ptr, arena->slab_size);
		}
		total += arena->slab_size;
	}
	if (arena->arena)
		munmap_checked(arena->arena, arena->prealloc);

	assert(total == arena->used);
}

void *
slab_map(struct slab_arena *arena)
{
	void *ptr;
	if ((ptr = lf_lifo_pop(&arena->cache))) {
		VALGRIND_MAKE_MEM_UNDEFINED(ptr, arena->slab_size);
		return ptr;
	}

	if (quota_use(arena->quota, arena->slab_size) < 0)
		return NULL;

	/** Need to allocate a new slab. */
	size_t used = pm_atomic_fetch_add(&arena->used, arena->slab_size);
	used += arena->slab_size;
	if (used <= arena->prealloc) {
		ptr = arena->arena + used - arena->slab_size;
		VALGRIND_MAKE_MEM_UNDEFINED(ptr, arena->slab_size);
		return ptr;
	}

	ptr = mmap_checked(arena->slab_size, arena->slab_size,
			   arena->flags);
	if (!ptr) {
		__sync_sub_and_fetch(&arena->used, arena->slab_size);
		quota_release(arena->quota, arena->slab_size);
	}

	madvise_checked(ptr, arena->slab_size, arena->flags);

	VALGRIND_MAKE_MEM_UNDEFINED(ptr, arena->slab_size);
	return ptr;
}

void
slab_unmap(struct slab_arena *arena, void *ptr)
{
	if (ptr == NULL)
		return;

	lf_lifo_push(&arena->cache, ptr);
	VALGRIND_MAKE_MEM_NOACCESS(ptr, arena->slab_size);
	VALGRIND_MAKE_MEM_DEFINED(lf_lifo(ptr), sizeof(struct lf_lifo));
}

void
slab_arena_mprotect(struct slab_arena *arena)
{
	if (arena->arena)
		mprotect(arena->arena, arena->prealloc, PROT_READ);
}
