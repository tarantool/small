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
#include <sys/uio.h>
#include <stdbool.h>

#include "slab_cache.h"
#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

enum {
	/** Maximum number of iovec vectors in obuf. */
	SMALL_OBUF_IOV_MAX = 128,
	/**
	 * Number of vectors at the beginning of iovec vector list
	 * that are allocated with distinct malloc call. Let's refer
	 * them as checked vectors.
	 */
	SMALL_OBUF_IOV_CHECKED_MAX = 96,
	/** Alignment for the checked vectors. */
	SMALL_OBUF_ALIGNMENT = 1,
	/** Minimum size when reserving memory. See struct obuf comment. */
	SMALL_OBUF_MIN_RESERVE = 128,
};

static_assert(SMALL_OBUF_IOV_MAX <= IOV_MAX,
	      "vector size should not exceed writev(2) limit");

/**
 * ASAN friendly implementation for obuf allocator. It has same interface as
 * regular implementation but at the beginning it uses malloc() for every
 * allocation. More precisely for the first (in terms of obuf_iovcnt())
 * SMALL_OBUF_IOV_CHECKED_MAX allocations. Then the memory is allocated in
 * exponentially growing blocks which then used to provide requested
 * allocations as in the regular implementation.
 *
 * This combined strategy allows to do out-of-bound access checks for first
 * allocations on the one hand and on the other hand limit the size of iov
 * vector list. The latter is required to avoid iov vector list reallocations
 * which is expected by some client code.
 *
 * See however a bit of limitation for out-of-bound access check in description
 * of small_asan_alloc.
 *
 * Allocations are completely not aligned in the beginning (that is not
 * 2 aligned). This improves unaligned memory access check.
 *
 * Also at the beginning we reserve at least SMALL_OBUF_MIN_RESERVE bytes
 * so that runtime code path with ASAN implementation is similar to regular
 * one. The overreserve is not very large comparing to regular one to reduce
 * memory waste.
 */
struct obuf {
	/** For compatibility with existing code only. */
	struct slab_cache *slabc;
	/** List of iovec vectors. NULL terminated. */
	struct iovec iov[SMALL_OBUF_IOV_MAX + 1];
	/**
	 * Vectors starting from SMALL_OBUF_IOV_CHECKED_MAX index are
	 * exponentially growing with factor 2 starting from this size.
	 * Note that size can be larger than the described if larger allocation
	 * is requested.
	 */
	size_t start_capacity;
	/** How many bytes are actually allocated for each iovec. */
	size_t capacity[SMALL_OBUF_IOV_MAX];
	/**
	 * If pos == 0 and iov[0] == NULL then obuf is empty. Otherwise
	 * pos is index of the vector in the vectors list holding last
	 * allocation. A bit odd but follows regular implementation
	 * semantics for compatibility.
	 */
	int pos;
	/** Total size of allocations. */
	size_t used;
	/** Whether memory is reserved with the obuf_reserve. */
	bool reserved;
};

void
obuf_create(struct obuf *buf, struct slab_cache *slabc, size_t start_capacity);

static inline bool
obuf_is_initialized(const struct obuf *buf)
{
	return buf->slabc != NULL;
}

static inline size_t
obuf_size(struct obuf *buf)
{
	return buf->used;
}

void *
obuf_reserve(struct obuf *buf, size_t size);

void *
obuf_alloc(struct obuf *buf, size_t size);

static inline struct obuf_svp
obuf_create_svp(struct obuf *buf)
{
	struct obuf_svp svp;
	svp.pos = buf->pos;
	svp.iov_len = buf->iov[buf->pos].iov_len;
	svp.used = buf->used;
	return svp;
}

static inline void *
obuf_svp_to_ptr(struct obuf *buf, struct obuf_svp *svp)
{
	return (char *)buf->iov[svp->pos].iov_base + svp->iov_len;
}

void
obuf_rollback_to_svp(struct obuf *buf, struct obuf_svp *svp);

void
obuf_destroy(struct obuf *buf);

void
obuf_reset(struct obuf *buf);

size_t
obuf_dup(struct obuf *buf, const void *data, size_t size);

static inline int
obuf_iovcnt(struct obuf *buf)
{
	return buf->iov[buf->pos].iov_base != NULL ? buf->pos + 1 : buf->pos;
}

static inline void *
obuf_reserve_cb(void *ctx, size_t *size)
{
	struct obuf *buf = (struct obuf *)ctx;
	void *ptr = obuf_reserve(buf, *size);
	*size = buf->capacity[buf->pos] - buf->iov[buf->pos].iov_len;
	return ptr;
}

static inline void *
obuf_alloc_cb(void *ctx, size_t size)
{
	return obuf_alloc((struct obuf *)ctx, size);
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */
