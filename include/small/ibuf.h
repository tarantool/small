#ifndef TARANTOOL_SMALL_IBUF_H_INCLUDED
#define TARANTOOL_SMALL_IBUF_H_INCLUDED
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
#include <stddef.h>
#include <assert.h>

#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/** @module Input buffer. */

struct slab_cache;

/*
 * Continuous piece of memory to store input.
 * Allocated in factors of 'start_capacity'.
 * Maintains position of the data "to be processed".
 *
 * Typical use case:
 *
 * struct ibuf *in;
 * coio_bread(coio, in, request_len);
 * if (ibuf_size(in) >= request_len) {
 *	process_request(in->rpos, request_len);
 *	in->rpos += request_len;
 * }
 */
struct ibuf
{
	struct slab_cache *slabc;
	char *buf;
	/** Start of input. */
	char *rpos;
	/** End of useful input */
	char *wpos;
	/** End of buffer. */
	char *end;
	size_t start_capacity;
};

void
ibuf_create(struct ibuf *ibuf, struct slab_cache *slabc, size_t start_capacity);

void
ibuf_destroy(struct ibuf *ibuf);

void
ibuf_reinit(struct ibuf *ibuf);

/** How much data is read and is not parsed yet. */
static inline size_t
ibuf_used(struct ibuf *ibuf)
{
	assert(ibuf->wpos >= ibuf->rpos);
	return ibuf->wpos - ibuf->rpos;
}

/** How much data can we fit beyond buf->wpos */
static inline size_t
ibuf_unused(struct ibuf *ibuf)
{
	assert(ibuf->wpos <= ibuf->end);
	return ibuf->end - ibuf->wpos;
}

/** How much memory is allocated */
static inline size_t
ibuf_capacity(struct ibuf *ibuf)
{
	return ibuf->end - ibuf->buf;
}

/**
 * Integer value of the position in the buffer - stable
 * in case of realloc.
 */
static inline size_t
ibuf_pos(struct ibuf *ibuf)
{
	assert(ibuf->buf <= ibuf->rpos);
	return ibuf->rpos - ibuf->buf;
}

static inline void
ibuf_poison_unallocated(const struct ibuf *ibuf)
{
	ASAN_POISON_MEMORY_REGION(ibuf->wpos, ibuf->end - ibuf->wpos);
}

static inline void
ibuf_unpoison_unallocated(const struct ibuf *ibuf)
{
	ASAN_UNPOISON_MEMORY_REGION(ibuf->wpos, ibuf->end - ibuf->wpos);
}

/** Forget all cached input. */
static inline void
ibuf_reset(struct ibuf *ibuf)
{
	ibuf->rpos = ibuf->wpos = ibuf->buf;
	ibuf_poison_unallocated(ibuf);
}

void *
ibuf_reserve_slow(struct ibuf *ibuf, size_t size);

static inline void *
ibuf_reserve(struct ibuf *ibuf, size_t size)
{
	if (ibuf->wpos + size <= ibuf->end) {
		ibuf_unpoison_unallocated(ibuf);
		return ibuf->wpos;
	}
	return ibuf_reserve_slow(ibuf, size);
}

static inline void *
ibuf_alloc(struct ibuf *ibuf, size_t size)
{
	void *ptr;
	if (ibuf->wpos + size <= ibuf->end) {
		/*
		 * In case of using same buffer we need to unpoison newly
		 * allocated memory after previous ibuf_alloc or poison after
		 * newly allocated memory after previous ibuf_reserve.
		 */
		ibuf_unpoison_unallocated(ibuf);
		ptr = ibuf->wpos;
	} else {
		ptr = ibuf_reserve_slow(ibuf, size);
		if (ptr == NULL)
			return NULL;
	}
	ibuf->wpos += size;
	ibuf_poison_unallocated(ibuf);
	return ptr;
}

/**
 * Shrink the buffer to the minimal possible capacity needed to store the data
 * written to the buffer and not yet consumed.
 */
void
ibuf_shrink(struct ibuf *ibuf);

/** Discard size bytes of data from write end of the buffer. */
static inline void
ibuf_discard(struct ibuf *ibuf, size_t size)
{
	assert(size <= ibuf_used(ibuf));
	ibuf->wpos -= size;
	ibuf_poison_unallocated(ibuf);
}

/**
 * Discard data written after position. Use ibuf_used to get position.
 * Note that you should not update read position in between. It is safe
 * to use if buffer is reallocated in between.
 */
static inline void
ibuf_truncate(struct ibuf *ibuf, size_t used)
{
	assert(used <= ibuf_used(ibuf));
	ibuf_discard(ibuf, ibuf_used(ibuf) - used);
}

static inline void *
ibuf_reserve_cb(void *ctx, size_t *size)
{
	struct ibuf *buf = (struct ibuf *) ctx;
	void *p = ibuf_reserve(buf, *size ? *size : buf->start_capacity);
	*size = ibuf_unused(buf);
	return p;
}

static inline void *
ibuf_alloc_cb(void *ctx, size_t size)
{
	return ibuf_alloc((struct ibuf *) ctx, size);
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* TARANTOOL_SMALL_IBUF_H_INCLUDED */
