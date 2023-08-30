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
#include "obuf.h"
#include "util.h"

#include <string.h>

void
obuf_create(struct obuf *buf, struct slab_cache *slabc, size_t start_capacity)
{
	buf->slabc = slabc;
	buf->start_capacity = start_capacity;
	memset(buf->iov, 0, lengthof(buf->iov) * sizeof(buf->iov[0]));
	memset(buf->capacity, 0,
	       lengthof(buf->capacity) * sizeof(buf->capacity[0]));
	buf->pos = 0;
	buf->used = 0;
	buf->reserved = false;
}

/**
 * Pointer to unallocated space. This is free space in the last allocated
 * buffer in buf->iov that is not allocated by user.
 */
static char *
obuf_unallocated_ptr(struct obuf *buf)
{
	return buf->iov[buf->pos].iov_base + buf->iov[buf->pos].iov_len;
}

/** Size of unallocated space. See obuf_unallocated_ptr. */
static size_t
obuf_unallocated_size(const struct obuf *buf)
{
	return buf->capacity[buf->pos] - buf->iov[buf->pos].iov_len;
}

/** Make sure size bytes are available as continuous chunk. */
static void
obuf_prepare_buf(struct obuf *buf, size_t size)
{
	size_t used = buf->iov[buf->pos].iov_len + size;
	if (buf->pos < SMALL_OBUF_IOV_CHECKED_MAX ||
	    used > buf->capacity[buf->pos]) {
		size_t capacity;
		if (buf->pos < SMALL_OBUF_IOV_CHECKED_MAX) {
			/* See limit explanation in the header. */
			if (size < SMALL_OBUF_MIN_RESERVE)
				capacity = SMALL_OBUF_MIN_RESERVE;
			else
				capacity = size;
		} else {
			capacity = buf->start_capacity;
			capacity <<= buf->pos - SMALL_OBUF_IOV_CHECKED_MAX;
			while (capacity < size)
				capacity <<= 1;
		}

		/* See obuf.pos semantics in struct definition. */
		if (buf->iov[buf->pos].iov_base != NULL)
			buf->pos++;
		small_asan_assert(buf->pos < SMALL_OBUF_IOV_MAX);

		char *alloc = small_asan_alloc(capacity,
					       SMALL_OBUF_ALIGNMENT, 0);
		char *payload = small_asan_payload_from_header(alloc);
		buf->iov[buf->pos].iov_base = payload;
		buf->iov[buf->pos].iov_len = 0;
		buf->capacity[buf->pos] = capacity;
	}
	ASAN_UNPOISON_MEMORY_REGION(obuf_unallocated_ptr(buf),
				    obuf_unallocated_size(buf));
}

void *
obuf_reserve(struct obuf *buf, size_t size)
{
	small_asan_assert(!buf->reserved);

	obuf_prepare_buf(buf, size);
	buf->reserved = true;
	return obuf_unallocated_ptr(buf);
}

void *
obuf_alloc(struct obuf *buf, size_t size)
{
	if (buf->reserved) {
		small_asan_assert(size <= obuf_unallocated_size(buf));
		buf->reserved = false;
	} else {
		obuf_prepare_buf(buf, size);
	}
	void *ptr = obuf_unallocated_ptr(buf);
	buf->iov[buf->pos].iov_len += size;
	buf->used += size;

	ASAN_POISON_MEMORY_REGION(obuf_unallocated_ptr(buf),
				  obuf_unallocated_size(buf));
	return ptr;
}

SMALL_NO_SANITIZE_ADDRESS void
obuf_rollback_to_svp(struct obuf *buf, struct obuf_svp *svp)
{
	small_asan_assert(svp->pos <= (size_t)buf->pos);
	int pos = svp->pos;
	/*
	 * Usually on rollback we start freeing from the position after the
	 * position in svp but in case of rollback to 0 we may want to
	 * free the iov[0]. This is due to inconvinient semantics of
	 * obuf.pos See obuf.pos semantics in struct definition.
	 */
	if (!(svp->pos == 0 &&
	      svp->iov_len == 0 &&
	      buf->iov[0].iov_base != NULL))
		pos++;

	for (int i = pos; i <= buf->pos; i++) {
		void *ptr = buf->iov[i].iov_base;
		small_asan_free(small_asan_header_from_payload(ptr));
	}

	size_t erase_size = buf->pos - pos + 1;
	memset(&buf->iov[pos], 0, sizeof(buf->iov[0]) * erase_size);
	memset(&buf->capacity[pos], 0, sizeof(buf->capacity[0]) * erase_size);
	buf->pos = svp->pos;
	buf->used = svp->used;
	buf->iov[buf->pos].iov_len = svp->iov_len;
	buf->reserved = false;
}

void
obuf_destroy(struct obuf *buf)
{
	struct obuf_svp svp;
	obuf_svp_reset(&svp);
	obuf_rollback_to_svp(buf, &svp);
	/* Safety and also makes mempool_is_initialized work. */
	memset(buf, 0, sizeof(*buf));
}

void
obuf_reset(struct obuf *buf)
{
	struct obuf_svp svp;
	obuf_svp_reset(&svp);
	obuf_rollback_to_svp(buf, &svp);
}

size_t
obuf_dup(struct obuf *buf, const void *data, size_t size)
{
	void *ptr = obuf_alloc(buf, size);
	memcpy(ptr, data, size);
	return size;
}
