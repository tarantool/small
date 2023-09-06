#pragma once
/*
 * Copyright 2022, Tarantool AUTHORS, please see AUTHORS file.
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
#include <unistd.h>
#include <stddef.h>
#include <assert.h>
#include <limits.h>

#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

/**
 * Helpers to provide the compiler with branch prediction information.
 */
#if __has_builtin(__builtin_expect) || defined(__GNUC__)
#  define small_likely(x)    __builtin_expect(!! (x),1)
#  define small_unlikely(x)  __builtin_expect(!! (x),0)
#else
#  define small_likely(x)    (x)
#  define small_unlikely(x)  (x)
#endif

/**
 * Add `always_inline` attribute to the function if possible. Add inline too.
 * Such function will always be inlined.
 *
 * Usage example:
 *
 * static SMALL_ALWAYS_INLINE void f() {}
 */
#if __has_attribute(always_inline) || defined(__GNUC__)
#  define SMALL_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#  define SMALL_ALWAYS_INLINE inline
#endif

/**
 * Return size of a memory page in bytes.
 */
static inline long
small_getpagesize(void)
{
	/* sysconf() returns -1 on error, or page_size >= 1 otherwise. */
	long page_size = sysconf(_SC_PAGESIZE);
	if (small_unlikely(page_size < 1))
		return 4096;
	return page_size;
}

/**
 * Align a size - round up to nearest divisible by the given alignment.
 * Alignment must be a power of 2
 */
static inline size_t
small_align(size_t size, size_t alignment)
{
	/* Must be a power of two */
	assert((alignment & (alignment - 1)) == 0);
	/* Bit arithmetics won't work for a large size */
	assert(size <= SIZE_MAX - alignment);
	return (size - 1 + alignment) & ~(alignment - 1);
}

/** Round up a number to the nearest power of two. */
static inline size_t
small_round(size_t size)
{
	if (size < 2)
		return size;
	assert(size <= SIZE_MAX / 2 + 1);
	assert(size - 1 <= ULONG_MAX);
	size_t r = 1;
	return r << (sizeof(unsigned long) * CHAR_BIT -
		     __builtin_clzl((unsigned long) (size - 1)));
}

/** Binary logarithm of a size. */
static inline size_t
small_lb(size_t size)
{
	assert(size <= ULONG_MAX);
	return sizeof(unsigned long) * CHAR_BIT -
		__builtin_clzl((unsigned long) size) - 1;
}
