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

#include "small_class.h"
#include <assert.h>
#include <math.h>

void
small_class_create(struct small_class *sc, unsigned granularity,
		   float desired_factor, unsigned min_alloc)
{
	assert(granularity > 0); /* size cannot be multiple of zero. */
	assert((granularity & (granularity - 1)) == 0); /* must power of 2. */
	assert(desired_factor > 1.f);
	assert(desired_factor <= 2.f);
	assert(min_alloc > 0); /* Cannot allocate zero. */

	sc->granularity = granularity;
	sc->ignore_bits_count = __builtin_ctz(granularity);
	float log2 = logf(2);
	sc->effective_bits = (unsigned)(logf(log2 / logf(desired_factor)) / log2 + .5);
	sc->effective_size = 1u << sc->effective_bits;
	sc->effective_mask = sc->effective_size - 1u;
	sc->size_shift = min_alloc - granularity;
	sc->size_shift_plus_1 = sc->size_shift + 1;

	sc->actual_factor = powf(2, 1.f / powf(2, sc->effective_bits));
}
