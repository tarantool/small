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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "small_config.h"

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

#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif

#ifndef offsetof
#  if __has_builtin(__builtin_offsetof)
#    define offsetof(type, member) __builtin_offsetof(type, member)
#  else
#    define offsetof(type, member) ((size_t)&((type *)0)->member)
#  endif
#endif /* offsetof */

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

#if !defined(__cplusplus) && !defined(static_assert)
#  define static_assert _Static_assert
#endif

#ifndef lengthof
#  define lengthof(array) (sizeof (array) / sizeof ((array)[0]))
#endif

#define small_xmalloc(size)							\
	({									\
		void *ret = malloc(size);					\
		if (small_unlikely(ret == NULL))				\
			small_xmalloc_fail(size, __FILE__, __LINE__);		\
		ret;								\
	})

/**
 * It is called on allocation failure in small_xmalloc. Print failure diagonstic
 * and exit with failure.
 */
void
small_xmalloc_fail(size_t size, const char *filename, int line);

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

/**
 * Align value to the nearest divisible by the given alignment which is
 * not greater than value. Alignment must be a power of 2.
 */
static inline size_t
small_align_down(size_t value, size_t alignment)
{
	assert((alignment & (alignment - 1)) == 0);
	return value & ~(alignment - 1);
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

#ifdef ENABLE_ASAN

#include <sanitizer/asan_interface.h>

/** This attribute is used by functions that access poisoned memory. */
#define SMALL_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))

/**
 * Similar to assert(3) but does not depend on NDEBUG.
 *
 * If expr is false then small_on_assert_failure callback is called.
 * Default callback prints diagnostic message and aborts.
 */
#define small_asan_assert(expr) do {						\
	if (small_unlikely(!(expr)))						\
		small_on_assert_failure(__FILE__, __LINE__, __func__, #expr);	\
} while (0)

typedef void (*small_on_assert_failure_f)(const char *filename, int line,
					  const char *funcname,
					  const char *expr);

/** Callback to be called if small_asan_assert is failed. */
extern small_on_assert_failure_f small_on_assert_failure;

/**
 * Default callback for small_asan_assert. Print diagnostic message and abort.
 */
void
small_on_assert_failure_default(const char *filename, int line,
				const char *funcname, const char *expr);

enum {
	/* Alignment of header in memory allocated with small_asan_alloc. */
	SMALL_ASAN_HEADER_ALIGNMENT = sizeof(void *),
	/*
	 * ASAN does not allow to precisely poison arbitrary ranges of memory.
	 * However if range end is 8-aligned or range end is end of memory
	 * allocated with malloc() then poison is precise.
	 */
	SMALL_POISON_ALIGNMENT = 8,
};

/** Random magic to be used for memory that cannot be poisoned. */
#define SMALL_ASAN_MAGIC 0xdeadbeefcafefeedUL

/**
 * Allocate aligned memory (payload) with extra place for header.
 *
 * Header has SMALL_ASAN_HEADER_ALIGNMENT (large enough alignment for use with
 * existing small allocators). Payload is aligned to `alignment`,
 * additionally payload is not aligned on 2 * `alignment`. This improves
 * unaligned access check.
 *
 * All allocated memory except for payload is poisoned. Note that due to poison
 * alignment restrictions we may not be able to poison part of alignment area
 * before payload. In this case we write magic to the area which cannot be
 * poisoned and check it is not changed when memory is freed.
 *
 * Return pointer to header. Use small_asan_payload_from_header to
 * get pointer to payload.
 */
void *
small_asan_alloc(size_t payload_size, size_t alignment, size_t header_size);

/**
 * Return pointer to payload given pointer to header allocated with
 * small_asan_alloc.
 */
static inline SMALL_NO_SANITIZE_ADDRESS void *
small_asan_payload_from_header(void *header)
{
	char *alloc = (char *)header - SMALL_ASAN_HEADER_ALIGNMENT;
	return (char *)header + *(uint16_t *)alloc;
}

/**
 * Return pointer to header given pointer to payload (for allocations
 * done thru small_asan_alloc).
 */

static inline SMALL_NO_SANITIZE_ADDRESS void *
small_asan_header_from_payload(void *payload)
{
	uint16_t offset;
	char *magic_begin = (char *)
		small_align_down((uintptr_t)payload, SMALL_POISON_ALIGNMENT);
	memcpy(&offset, (char *)magic_begin - sizeof(offset), sizeof(offset));
	return (char *)payload - offset;
}

/** Free memory allocated with small_asan_alloc. Takes pointer to header. */
void
small_asan_free(void *header);

/** Reserve a runtime unique id. */
unsigned int
small_asan_reserve_id(void);

#else /* ifndef ENABLE_ASAN */

#define ASAN_POISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))

#endif /* ifndef ENABLE_ASAN */
