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
#include <stdlib.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

enum {
	/** Alignment for slabs from ASAN slab arena. */
	SMALL_ARENA_SLAB_ALIGNMENT = sizeof(long long),
};

/**
 * ASAN friendly implementation for region allocator. It has same
 * interface as regular implementation but allocates every allocation using
 * malloc(). This allows to do usual ASAN checks for memory allocation.
 *
 * Note that this implementation does not provide same alignment as regular.
 * Allocation alignment is just malloc allocation alignment.
 */
struct slab_arena {
	/**
	 * Allocation quota. It is not used directly. We need to keep quota
	 * to pass it to slab cache only.
	 */
	struct quota *quota;
	/** Slab size. */
	uint32_t slab_size;
	/** Just stub. Always 0. */
	size_t used;
	/** Unique id among all allocators. */
	unsigned int id;
};

/** Extra data associated with each slab. */
struct slab_arena_object {
	/** id of the arena that this object belongs to. */
	unsigned int arena_id;
};

int
slab_arena_create(struct slab_arena *arena, struct quota *quota,
		  size_t prealloc, uint32_t slab_size, int flags);

static inline void
slab_arena_destroy(struct slab_arena *arena)
{
	(void)arena;
}

void *
slab_map(struct slab_arena *arena);

void
slab_unmap(struct slab_arena *arena, void *ptr);

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
