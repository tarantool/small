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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <sys/mman.h>

#include "features.h"
#include "config.h"

typedef bool (*rt_helper_t)(void);

#define SMALL_FEATURE_MASK(v)	((uint64_t)1 << (v))

/* The mask carries bits for compiled in features */
static uint64_t builtin_mask =
#ifdef TARANTOOL_SMALL_USE_MADVISE
	SMALL_FEATURE_MASK(SMALL_FEATURE_DONTDUMP)	|
#endif
	0;

#ifdef TARANTOOL_SMALL_USE_MADVISE
static bool
test_dontdump(void)
{
	size_t size = sysconf(_SC_PAGESIZE);
	intptr_t ignore_it;
	bool ret = false;
	char buf[64];
	void *ptr;

	(void)ignore_it;

	/*
	 * We need to try madvise a real data,
	 * plain madvise() with -ENOSYS is not
	 * enough: we need a specific flag to
	 * be implemented. Thus allocate page
	 * and work on it.
	 */

	ptr = mmap(NULL, size, PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) {
		/*
		 * We're out of memory, and cant guarantee anything.
		 */
		ignore_it = (intptr_t)strerror_r(errno, buf, sizeof(buf));
		fprintf(stderr, "Error in mmap(NULL, %zu, ...): %s\n", size, buf);
		goto out;
	}

	if (madvise(ptr, size, MADV_DONTDUMP) == 0)
		ret = true;

	if (munmap(ptr, size)) {
		ignore_it = (intptr_t)strerror_r(errno, buf, sizeof(buf));
		fprintf(stderr, "Error in munmap(%p, %zu): %s\n",
			ptr, size, buf);
	}
out:
	return ret;
}
#else
static bool test_dontdump(void) { return false; }
#endif

/*
 * Runtime testers, put there features if they are dynamic.
 */
static rt_helper_t rt_helpers[FEATURE_MAX] = {
	[SMALL_FEATURE_DONTDUMP]	= test_dontdump,
};

/**
 * small_test_feature -- test if particular feature is supported
 * @feature: A feature to test.
 *
 * Returns true if feature is available, false othrewise.
 */
bool
small_test_feature(unsigned int feature)
{
	uint64_t mask = SMALL_FEATURE_MASK(feature);

	if (feature >= FEATURE_MAX)
		return false;

	/* Make sure it is compiled in. */
	if (!(builtin_mask & mask))
		return false;

	/*
	 * Some feature may require runtime
	 * testing (say we're compiled with
	 * particular feature support but
	 * it is unavailable on the system
	 * we're running on, or get filtered
	 * out).
	 */
	return rt_helpers[feature] ? rt_helpers[feature]() : true;
}
