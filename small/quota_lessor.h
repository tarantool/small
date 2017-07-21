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
 * Quota lessor allows to reduce strict usage of quota. The quota
 * has two shortcomings:
 * 1. Its precision is 1Kb.
 * 2. It is thread-safe and use atomics - it is too slow for
 *    frequent quota_use calls from different threads.
 *
 * The quota lessor uses the original quota to account big memory
 * blocks, at least 1Mb each. Then the lessor reaccounts this
 * memory in small and frequent allocs, but with no original qouta
 * access.
 * The end of lease is implemented in the similar way - the lessor
 * does not release small bytes count. The lessor accumulates
 * freed memory until it reaches at least 1Mb, an then releases.
 *
 * In such way the lessor decreases atomic locks usage and
 * improves the precision from 1024 bytes to 1 byte.
 * It is not thread-safe, so per each thread must be a lessor
 * created.
 */

/** Best byte count to alloc from original quota. */
#define QUOTA_LEASE_SIZE (QUOTA_UNIT_SIZE * 1024)

struct quota_lessor {
	/** Original thread-safe, 1Kb precision quota. */
	struct quota *source;
	/** Byte count taken from @source, but not used. */
	size_t available;
	/** Used byte count. */
	size_t leased;
};

static inline size_t
quota_leased(const struct quota_lessor *lessor)
{
	return lessor->leased;
}

static inline size_t
quota_available(const struct quota_lessor *lessor)
{
	return lessor->available;
}

static inline void
quota_lessor_create(struct quota_lessor *lessor, struct quota *source)
{
	lessor->source = source;
	lessor->available = 0;
	lessor->leased = 0;
	assert(quota_total(source) >= QUOTA_LEASE_SIZE);
}

/**
 * @retval  0 Success.
 * @retval -1 Error, not enough quota.
 */
static inline int
quota_lease(struct quota_lessor *lessor, size_t size)
{
	/* Fast way, there is enough unused quota. */
	if (lessor->available >= size) {
		lessor->available -= size;
		lessor->leased += size;
		return 0;
	}

	/* Need to use original quota. */
	size_t needed = size - lessor->available;
	/* quota_use takes size multiple to QUOTA_UNIT_SIZE. */
	size_t mult_size;
	if (needed % QUOTA_UNIT_SIZE == 0)
		mult_size = needed;
	else
		mult_size = needed + QUOTA_UNIT_SIZE - needed % QUOTA_UNIT_SIZE;
	assert(mult_size % QUOTA_UNIT_SIZE == 0);
	size_t best_size;
	if (QUOTA_LEASE_SIZE > mult_size)
		best_size = QUOTA_LEASE_SIZE;
	else
		best_size = mult_size;
	while(true) {
		ssize_t source_mem =
			quota_use(lessor->source, best_size);
		if (source_mem != -1) {
			assert((size_t)source_mem == best_size);
			/*
			 * 1. available_new = available_old +
			 *                    source_mem - size;
			 * 2. available_old - size == -needed;
			 * 3. available_new = source_mem - needed.
			 */
			lessor->available = source_mem - needed;
			lessor->leased += size;
			return 0;
		}
		/* Cannot allocate less, than mult_size. */
		if (best_size == mult_size)
			return -1;
		assert(best_size > mult_size);
		/*
		 * Divide the needed size, until the original
		 * quota successfully returns it.
		 */
		best_size /= 2;
		if (best_size < mult_size)
			best_size = mult_size;
	}
	assert(0);
}

static inline void
quota_end_lease(struct quota_lessor *lessor, size_t size)
{
	lessor->available += size;
	assert(size <= lessor->leased);
	lessor->leased -= size;
	/*
	 * Release the original quota, when enough bytes
	 * accumulated to avoid frequent quota_release calls.
	 */
	if (lessor->available >= 2 * QUOTA_LEASE_SIZE) {
		size_t to_release = lessor->available -
				    lessor->available % QUOTA_UNIT_SIZE;
		/* Leave one LEASE_SIZE to avoid oscillation. */
		to_release -= QUOTA_LEASE_SIZE;
		quota_release(lessor->source, to_release);
		assert(lessor->available >= to_release);
		lessor->available -= to_release;
	}
}

static inline void
quota_end_total(struct quota_lessor *lessor)
{
	assert(lessor->leased == 0);
	assert(lessor->available % QUOTA_UNIT_SIZE == 0);
	quota_release(lessor->source, lessor->available);
	lessor->available = 0;
}

#endif /* INCLUDES_TARANTOOL_SMALL_QUOTA_LESSOR_H */
