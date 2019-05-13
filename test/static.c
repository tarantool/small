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
#include "small/static.h"
#include "unit.h"

static inline void
check_static_alloc(size_t size, size_t first_pos, size_t end_pos)
{
	char *b = static_alloc(size);
	is(b, static_storage_buffer + first_pos, "allocated %d from %d",
	   (int) size, (int) first_pos);
	is(static_storage_pos, end_pos, "to %d", (int) end_pos);
}

static void
test_unaligned(void)
{
	header();
	plan(15);
	static_reset();

	check_static_alloc(10, 0, 10);
	int offset = 10;
	int size = SMALL_STATIC_SIZE / 2;
	check_static_alloc(size, offset, offset + size);
	offset += size;

	size = SMALL_STATIC_SIZE / 3;
	check_static_alloc(size, offset, offset + size);
	offset += size;

	size = SMALL_STATIC_SIZE - offset;
	check_static_alloc(size, offset, offset + size);

	size = 1;
	offset = 0;
	check_static_alloc(size, offset, offset + size);

	char *a = static_reserve(300);
	char *b = static_alloc(100);
	char *c = static_alloc(153);
	char *d = static_alloc(47);
	ok(a == b && c == b + 100 && d == c + 153,
	   "big reserve can be consumed in multiple allocs");

	is(static_alloc(SMALL_STATIC_SIZE), static_storage_buffer,
	   "can allocate the entire buffer");
	is(static_storage_pos, SMALL_STATIC_SIZE, "position is updated");

	is(static_alloc(SMALL_STATIC_SIZE + 1), NULL,
	   "can't allocate more - the memory is static and can't be extended");
	is(static_storage_pos, SMALL_STATIC_SIZE, "position is not changed");

	check_plan();
	footer();
}

static void
test_aligned(void)
{
	header();
	plan(17);
	static_reset();
	size_t alignment = 8;

	char *p = static_aligned_reserve(0, alignment);
	is(p, &static_storage_buffer[0], "aligned reserve 0");
	is(static_storage_pos, 0, "position is not changed");

	p = static_alloc(1);
	is(p, &static_storage_buffer[0], "alloc 1");
	is(static_storage_pos, 1, "position is 1");

	p = static_aligned_alloc(3, alignment);
	is(p, &static_storage_buffer[8], "aligned alloc 3");
	is(static_storage_pos, 11, "position is changed to aligned pos + size");

	p = static_alloc(2);
	is(p, &static_storage_buffer[11], "alloc 2");
	is(static_storage_pos, 13, "position is changed to + size, "\
	   "no alignment");

	p = static_aligned_reserve(53, alignment);
	is(p, &static_storage_buffer[16], "aligned reserve 53");
	is(static_storage_pos, 13, "position is not changed");

	p = static_aligned_alloc(53, alignment);
	is(p, &static_storage_buffer[16], "aligned alloc 53");
	is(static_storage_pos, 69, "position is changed to aligned pos + size");

	p = static_aligned_alloc(100, alignment);
	is(p, &static_storage_buffer[72], "aligned alloc 100");
	is(static_storage_pos, 172, "position is changed to aligned pos + size")

	static_alloc(SMALL_STATIC_SIZE - static_storage_pos - 13);
	p = static_aligned_alloc(10, alignment);
	is(p, &static_storage_buffer[0], "aligned alloc 10, when 13 is "\
	   "available, alignment wrapped the buffer");
	is(static_storage_pos, 10, "position is changed to aligned pos + size");

	static_alloc(SMALL_STATIC_SIZE - static_storage_pos - 13);
	p = static_aligned_reserve(6, alignment);
	is(p, static_aligned_reserve(6, alignment),
	   "the same reserve returns the same address");

	check_plan();
	footer();
}

int
main(void)
{
	header();
	plan(2);

	test_unaligned();
	test_aligned();

	int rc = check_plan();
	footer();
	return rc;
}
