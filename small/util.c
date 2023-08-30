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
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

void
small_xmalloc_fail(size_t size, const char *filename, int line)
{
	fprintf(stderr, "Can't allocate %zu bytes at %s:%d",
		size, filename, line);
	exit(EXIT_FAILURE);
}

#ifdef ENABLE_ASAN

void
small_on_assert_failure_default(const char *filename, int line,
			        const char *funcname, const char *expr)
{
	fprintf(stderr, "%s:%d: %s: Assertion `%s' failed.\n",
		filename, line, funcname, expr);
	abort();
}

small_on_assert_failure_f small_on_assert_failure =
						small_on_assert_failure_default;

void *
small_asan_alloc(size_t payload_size, size_t alignment, size_t header_size)
{
	static_assert(sizeof(uint16_t) <= SMALL_ASAN_HEADER_ALIGNMENT,
		      "offset is not fit before header");
	/**
	 * Allocated memory has next structure:
	 *
	 * 1. place for offset from header to payload
	 * 2. header
	 * 3. padding due to payload alignment
	 * 4. place for offset from payload to header
	 * 5. payload
	 * 5. unused space due to payload padding
	 *
	 * Note that although offset is limited to 2 bytes field (1)
	 * has SMALL_ASAN_HEADER_ALIGNMENT size so that header is aligned
	 * to this value.
	 */
	size_t alloc_size = SMALL_ASAN_HEADER_ALIGNMENT +
			    header_size +
			    SMALL_POISON_ALIGNMENT - 1 +
			    sizeof(uint16_t) +
			    2 * alignment - 1 +
			    payload_size;
	char *alloc = (char *)small_xmalloc(alloc_size);

	char *payload = alloc + alloc_size - payload_size;
	payload = (char *)small_align_down((uintptr_t)payload, alignment);
	if (((uintptr_t)payload % (2 * alignment)) == 0)
		payload -= alignment;

	char *header = alloc + SMALL_ASAN_HEADER_ALIGNMENT;
	small_asan_assert(payload - header <= UINT16_MAX);
	uint16_t offset = payload - header;
	char *magic_begin = (char *)
		small_align_down((uintptr_t)payload, SMALL_POISON_ALIGNMENT);
	memcpy(magic_begin - sizeof(offset), &offset, sizeof(offset));
	*(uint16_t *)alloc = offset;

	/* Poison area after the payload. */
	char *payload_end = payload + payload_size;
	char *alloc_end = alloc + alloc_size;
	ASAN_POISON_MEMORY_REGION(payload_end, alloc_end - payload_end);
	/* Poison area before the payload. */
	ASAN_POISON_MEMORY_REGION(alloc, magic_begin - alloc);
	static_assert(sizeof(SMALL_ASAN_MAGIC) >= SMALL_POISON_ALIGNMENT,
		      "magic size is less than poison alignment");
	uint64_t magic = SMALL_ASAN_MAGIC;
	static_assert(sizeof(SMALL_ASAN_MAGIC) == sizeof(magic),
		      "magic size is less than container");
	/* Write magic to the area that cannot be poisoned. */
	memcpy(magic_begin, &magic, payload - magic_begin);

	return header;
}

void
small_asan_free(void *header)
{
	char *alloc = (char *)header - SMALL_ASAN_HEADER_ALIGNMENT;
	char *payload = (char *)small_asan_payload_from_header(header);
	char *magic_begin = (char *)small_align_down((uintptr_t)payload,
						     SMALL_POISON_ALIGNMENT);
	uint64_t magic = SMALL_ASAN_MAGIC;
	small_asan_assert(memcmp(magic_begin, &magic,
				 payload - magic_begin) == 0 &&
			  "corrupted magic");
	free(alloc);
}

#endif /* ifdef ENABLE_ASAN */
