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
#include "slab_arena.h"
#include "util.h"

int
slab_arena_create(struct slab_arena *arena, struct quota *quota,
		  size_t prealloc, uint32_t slab_size, int flags)
{
	(void)prealloc;
	(void)flags;
	(void)prealloc;
	arena->quota = quota;
	if (slab_size < SLAB_MIN_SIZE)
		slab_size = SLAB_MIN_SIZE;
	arena->slab_size = small_round(slab_size);
	arena->id = small_asan_reserve_id();
	arena->used = 0;
	return 0;
}

SMALL_NO_SANITIZE_ADDRESS void *
slab_map(struct slab_arena *arena)
{
	struct slab_arena_object *obj =
			small_asan_alloc(arena->slab_size,
					 SMALL_ARENA_SLAB_ALIGNMENT,
					 sizeof(obj));
	obj->arena_id = arena->id;
	return small_asan_payload_from_header(obj);
}

SMALL_NO_SANITIZE_ADDRESS void
slab_unmap(struct slab_arena *arena, void *ptr)
{
	if (ptr == NULL)
		return;
	struct slab_arena_object *obj = small_asan_header_from_payload(ptr);
	small_asan_assert(obj->arena_id == arena->id &&
			  "object and arena id mismatch");
	small_asan_free(obj);
}
