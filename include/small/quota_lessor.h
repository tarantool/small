#ifndef INCLUDES_TARANTOOL_SMALL_QUOTA_LESSOR_H
#define INCLUDES_TARANTOOL_SMALL_QUOTA_LESSOR_H
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
#include "quota.h"
/**
 * Quota lessor is a convenience wrapper around thread-safe `struct quota`
 * to allocate small chunks of memory from the single thread. Original quota
 * has 1Kb precision and uses atomics, which are too slow for frequent calls
 * from different threads.
 *
 * The quota lessor allocates huge (1Mb+) chunks of memory from
 * the source quota and then leases small chunks to the end users.
 * The end of lease is implemented in the similar way - the lessor
 * does not release small amounts, but accumulates freed memory until
 * it reaches at least 1Mb an then releases it.
 *
 * This decreases usage of atomic locks and improves quota
 * precision from 1024 bytes to 1 byte. This class, however, is
 * not thread-safe, so there must be a lessor in each thread.
 */
struct quota_lessor {
	/** Original thread-safe, 1Kb precision quota. */
	struct quota *source;
	/** The number of bytes taken from @a source. */
	size_t used;
	/** The number of bytes leased. */
	size_t leased;
};

/**
 * Return the total number of bytes leased
 * @param lessor quota_lessor
 */
static inline size_t
quota_leased(const struct quota_lessor *lessor)
{
	return lessor->leased;
}

/**
 * Return the number of bytes allocated from the source, but not leased yet
 * @param lessor quota_lessor
 */
static inline size_t
quota_available(const struct quota_lessor *lessor)
{
	return lessor->used - lessor->leased;
}

/** Min byte count to alloc from original quota. */
#define QUOTA_USE_MIN (QUOTA_UNIT_SIZE * 1024)

/**
 * Create a new quota lessor from @a source.
 * @param lessor quota_lessor
 * @param source source quota
 */
static inline void
quota_lessor_create(struct quota_lessor *lessor, struct quota *source)
{
	lessor->source = source;
	lessor->used = 0;
	lessor->leased = 0;
	assert(quota_total(source) >= QUOTA_USE_MIN);
}

/**
 * Destroy the quota lessor
 * @param lessor quota_lessor
 * @pre quota_leased(lessor) == 0
 */
static inline void
quota_lessor_destroy(struct quota_lessor *lessor)
{
	assert(lessor->leased == 0);
	if (lessor->used == 0)
		return;
	assert(lessor->used % QUOTA_UNIT_SIZE == 0);
	quota_release(lessor->source, lessor->used);
	lessor->used = 0;
}

/**
 * Lease @a size bytes.
 * @param lessor quota lessor
 * @param size the number of bytes to lease
 * @retval  >= 0 Number of leased bytes.
 * @retval -1 Error, not enough quota.
 */
static inline ssize_t
quota_lease(struct quota_lessor *lessor, ssize_t size)
{
	/* Fast way, there is enough unused quota. */
	if (lessor->leased + size <= lessor->used) {
		lessor->leased += size;
		return size;
	}
	/* Need to use the original quota. */
	size_t required = size + lessor->leased - lessor->used;
	size_t use = required > QUOTA_USE_MIN ? required : QUOTA_USE_MIN;

	for (; use >= required; use = use/2) {

		ssize_t used = quota_use(lessor->source, use);
		if (used >= 0) {
			lessor->used += used;
			lessor->leased += size;
			return size;
		}
	}
	return -1;
}

/*
 * End the lease of @a size bytes
 * @param lessor quota_lessor
 * @param size the number of bytes to return
 */
static inline ssize_t
quota_end_lease(struct quota_lessor *lessor, size_t size)
{
	assert(lessor->leased >= size);
	lessor->leased -= size;
	size_t available = lessor->used - lessor->leased;
	/*
	 * Release the original quota when enough bytes
	 * accumulated to avoid frequent quota_release() calls.
	 */
	if (available >= 2 * QUOTA_USE_MIN) {
		/* Do not release too much to avoid oscillation. */
		size_t release = available - QUOTA_USE_MIN - QUOTA_UNIT_SIZE;
		lessor->used -= quota_release(lessor->source, release);
	}
	return size;
}

#endif /* INCLUDES_TARANTOOL_SMALL_QUOTA_LESSOR_H */
