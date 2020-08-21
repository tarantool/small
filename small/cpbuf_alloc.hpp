#ifndef TARANTOOL_CPBUF_ALLOC_H_INCLUDED
#define TARANTOOL_CPBUF_ALLOC_H_INCLUDED
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
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
#include "slab_arena.h"
#include "quota.h"

/**
 * Allocator which is based on slab cache allocator. It returns chunks of
 * memory exactly of size N where N is slab size.
 */
template <size_t N>
class SlabAllocatorInternals
{
public:
	SlabAllocatorInternals()
	{
		quota_init(&m_quota, QUOTA_MAX);
		slab_arena_create(&m_arena, &m_quota, 0, N, SLAB_ARENA_PRIVATE);
		slab_cache_create(&m_cache, &m_arena);
	}
	static struct quota m_quota;
	static struct slab_arena m_arena;
	static struct slab_cache m_cache;
};

template <size_t N>
class SlabAllocator
{
	/**
	 * To get all benefits from slab allocator size of single allocation
	 * must be equal to the power of two.
	 */
	static_assert((N & (N - 1)) == 0);
public:
	static char *alloc();
	static void free(char *ptr);
	static constexpr size_t REAL_SIZE = N - slab_sizeof();
private:
	static SlabAllocatorInternals<N> mInternals;
};

template <size_t N>
char *
SlabAllocator<N>::alloc()
{
	return ((char *) slab_get(&mInternals.m_cache, N - slab_sizeof()) + slab_sizeof());
}

template <size_t N>
void
SlabAllocator<N>::free(char *ptr)
{
	slab_put(&mInternals.m_cache, (struct slab *) (ptr - slab_sizeof()));
}

#endif /* TARANTOOL_CPBUF_ALLOC_H_INCLUDED */
