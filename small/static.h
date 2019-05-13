#ifndef TARANTOOL_SMALL_STATIC_H_INCLUDED
#define TARANTOOL_SMALL_STATIC_H_INCLUDED
/*
 * Copyright 2010-2019, Tarantool AUTHORS, please see AUTHORS file.
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
#include <string.h>
#include "slab_arena.h"

enum {
	/**
	 * Two pages (8192) would be too small for arbitrary
	 * cases, but plus even 1 byte would occupy the whole new
	 * page anyway, so here it is 3 pages.
	 */
	SMALL_STATIC_SIZE = 4096 * 3,
};

/**
 * Thread-local statically allocated temporary BSS resident
 * cyclic buffer.
 */
extern __thread char static_storage_buffer[SMALL_STATIC_SIZE];
/** Next free position in the buffer. */
extern __thread size_t static_storage_pos;

/**
 * Reset the static buffer to its initial state. Can be used, for
 * example, if there are many reserve + alloc calls, and a caller
 * needs them contiguous. Reset can prevent buffer position wrap.
 */
static inline void
static_reset(void)
{
	static_storage_pos = 0;
}

/**
 * Return a pointer onto the static buffer with at least @a size
 * continuous bytes. When @a size is bigger than the static buffer
 * size, then NULL is returned always. This is due to the fact the
 * buffer is stored in BSS section - it is not dynamic and can't
 * be extended. If there is not enough space till the end of the
 * buffer, then it is recycled.
 */
static inline void *
static_reserve(size_t size)
{
	if (static_storage_pos + size > SMALL_STATIC_SIZE) {
		if (size > SMALL_STATIC_SIZE)
			return NULL;
		static_storage_pos = 0;
	}
	return &static_storage_buffer[static_storage_pos];
}

/**
 * Reserve and propagate buffer position so as next allocations
 * would not get the same pointer until the buffer is recycled.
 */
static inline void *
static_alloc(size_t size)
{
	void *res = static_reserve(size);
	if (res != NULL)
		static_storage_pos += size;
	return res;
}

/**
 * The same as reserve, but a result address is aligned by @a
 * alignment.
 */
static inline void *
static_aligned_reserve(size_t size, size_t alignment)
{
	void *unaligned = static_reserve(size + alignment - 1);
	if (unaligned == NULL)
		return NULL;
	return (void *) small_align((size_t) unaligned, alignment);
}

/**
 * The same as alloc, but a result address is aligned by @a
 * alignment.
 */
static inline void *
static_aligned_alloc(size_t size, size_t alignment)
{
	void *res = static_aligned_reserve(size, alignment);
	if (res != NULL) {
		/*
		 * Aligned reserve could add a padding. Restore
		 * the position.
		 */
		static_storage_pos = (char *) res - &static_storage_buffer[0];
		static_storage_pos += size;
	}
	return res;
}

#endif /* TARANTOOL_SMALL_STATIC_H_INCLUDED */
