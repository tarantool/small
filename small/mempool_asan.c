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
#include "mempool.h"

enum {
	/** Maximum alignment for the ASAN mempool allocation. */
	MEMPOOL_ASAN_MAX_ALIGNMENT = 4096,
};

void
mempool_create(struct mempool *pool, struct slab_cache *cache,
	       uint32_t objsize)
{
	(void)cache;
	small_asan_assert(objsize > 0);
	pool->objsize = objsize;
	pool->objcount = 0;

	/* Alignment is explained in header. */
	pool->alignment = 1 << (__builtin_ffs(objsize) - 1);
	if (pool->alignment > MEMPOOL_ASAN_MAX_ALIGNMENT)
		pool->alignment = MEMPOOL_ASAN_MAX_ALIGNMENT;

	pool->id = small_asan_reserve_id();
}

void
mempool_destroy(struct mempool *pool)
{
	/* Safety and also makes mempool_is_initialized work. */
	memset(pool, 0, sizeof(*pool));
}

SMALL_NO_SANITIZE_ADDRESS void *
mempool_alloc(struct mempool *pool)
{

	struct mempool_object *obj =
			small_asan_alloc(pool->objsize, pool->alignment,
					 sizeof(*obj));
	obj->pool_id = pool->id;
	pool->objcount++;

	return small_asan_payload_from_header(obj);
}

SMALL_NO_SANITIZE_ADDRESS void
mempool_free(struct mempool *pool, void *ptr)
{
	struct mempool_object *obj = small_asan_header_from_payload(ptr);
	small_asan_assert(obj->pool_id == pool->id &&
			  "object and pool id mismatch");
	pool->objcount--;

	small_asan_free(obj);
}
