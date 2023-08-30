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
#include "slab_cache.h"
#include "util.h"

void
slab_cache_create(struct slab_cache *cache, struct slab_arena *arena)
{
	quota_lessor_create(&cache->quota, arena->quota);
	cache->id = small_asan_reserve_id();
	slab_cache_set_thread(cache);
}

SMALL_NO_SANITIZE_ADDRESS struct slab *
slab_get(struct slab_cache *cache, size_t size)
{
	small_asan_assert(pthread_equal(cache->thread_id, pthread_self()));
	size = slab_real_size(cache, size);
	struct slab_cache_object *obj =
			small_asan_alloc(size,
					 SMALL_CACHE_SLAB_ALIGNMENT,
					 sizeof(obj));
	obj->cache_id = cache->id;
	struct slab *slab = small_asan_payload_from_header(obj);
	slab->size = size;
	cache->used += size;
	return slab;
}

SMALL_NO_SANITIZE_ADDRESS void
slab_put(struct slab_cache *cache, struct slab *slab)
{
	small_asan_assert(pthread_equal(cache->thread_id, pthread_self()));
	struct slab_cache_object *obj = small_asan_header_from_payload(slab);
	small_asan_assert(obj->cache_id == cache->id &&
			  "object and cache id mismatch");
	cache->used -= slab->size;
	small_asan_free(obj);
}
