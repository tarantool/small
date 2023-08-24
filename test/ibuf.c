#include <small/quota.h>
#include <small/ibuf.h>
#include <small/slab_cache.h>
#include <stdio.h>
#include <string.h>
#include "unit.h"

struct slab_cache cache;
struct slab_arena arena;
struct quota quota;

static void
test_ibuf_basic(void)
{
	plan(6);
	header();

	struct ibuf ibuf;
	ibuf_create(&ibuf, &cache, 16320);
	ok(ibuf_used(&ibuf) == 0);
	void *ptr = ibuf_alloc(&ibuf, 10);
	ok(ptr != NULL);
	ok(ibuf_used(&ibuf) == 10);

	ptr = ibuf_alloc(&ibuf, 1000000);
	ok(ptr != NULL);
	ok(ibuf_used(&ibuf) == 1000010);

	ibuf_reset(&ibuf);
	ok(ibuf_used(&ibuf) == 0);

	footer();
	check_plan();
}

static void
test_ibuf_shrink(void)
{
	plan(12);
	header();

	struct ibuf ibuf;
	const size_t start_capacity = 16 * 1024;
	ibuf_create(&ibuf, &cache, start_capacity);
	ok(ibuf_alloc(&ibuf, 100 * 1024) != NULL);
	/*
	 * Check that ibuf is not shrunk lower than ibuf_used().
	 */
	ibuf.rpos += 70 * 1024;
	ibuf_shrink(&ibuf);
	ok(ibuf_used(&ibuf) == (100 - 70) * 1024);
	ok(ibuf_capacity(&ibuf) >= ibuf_used(&ibuf));
	ok(ibuf_capacity(&ibuf) < start_capacity * 4);
	/*
	 * Check that there is no relocation if the actual size of the new slab
	 * equals the old slab size.
	 */
	ibuf.rpos++;
	char *prev_buf = ibuf.buf;
	ibuf_shrink(&ibuf);
	ok(prev_buf == ibuf.buf);
	/*
	 * Check that ibuf is not shrunk lower than start_capacity.
	 */
	ibuf.rpos = ibuf.wpos - 1;
	ibuf_shrink(&ibuf);
	ok(ibuf_capacity(&ibuf) >= start_capacity);
	ok(ibuf_capacity(&ibuf) < start_capacity * 2);
	/*
	 * Check that empty ibuf is shrunk to the zero capacity.
	 */
	ibuf.rpos = ibuf.wpos;
	ibuf_shrink(&ibuf);
	ok(ibuf_capacity(&ibuf) == 0);
	/*
	 * Check that ibuf_shrink() does shrink large "unordered" slabs,
	 * i.e. allocated by slab_get_large().
	 */
	ok(ibuf_alloc(&ibuf, 9 * 1024 * 1024) != NULL);
	ok(ibuf_capacity(&ibuf) == 16 * 1024 * 1024);
	ibuf.rpos += 2 * 1024 * 1024;
	ibuf_shrink(&ibuf);
	ok(ibuf_capacity(&ibuf) == 7 * 1024 * 1024);
	/*
	 * Check that there is no relocation if the size of a large slab
	 * doesn't change.
	 */
	prev_buf = ibuf.buf;
	ibuf_shrink(&ibuf);
	ok(prev_buf == ibuf.buf);

	footer();
	check_plan();
}

static void
test_ibuf_truncate()
{
	plan(4);
	header();

	char *ptr;
	const char *hello = "Hello Hello";
	const char *goodbye = "Goodbye";
	struct ibuf ibuf;

	ibuf_create(&ibuf, &cache, 16 * 1024);
	ibuf_alloc(&ibuf, 10);
	ibuf.rpos += 10;
	ptr = ibuf_alloc(&ibuf, strlen(hello) + 1);
	fail_unless(ptr != NULL);
	strcpy(ptr, hello);
	size_t svp = ibuf_used(&ibuf);

	/*
	 * Test when there is NO reallocation in between used/truncate.
	 */
	ptr = ibuf_alloc(&ibuf, 100);
	fail_unless(ptr != NULL);
	strcpy(ptr, goodbye);
	ibuf_truncate(&ibuf, svp);
	ok(ibuf_used(&ibuf) == svp);
	ok(strcmp(ibuf.rpos, hello) == 0);

	/*
	 * Test when there IS reallocation in between used/truncate.
	 */
	ptr = ibuf_alloc(&ibuf, 32 * 1024);
	fail_unless(ptr != NULL);
	strcpy(ptr, goodbye);
	ibuf_truncate(&ibuf, svp);
	ok(ibuf_used(&ibuf) == svp);
	ok(strcmp(ibuf.rpos, hello) == 0);

	footer();
	check_plan();
}

int main()
{
	plan(3);
	header();

	quota_init(&quota, UINT_MAX);
	slab_arena_create(&arena, &quota, 0,
			  4000000, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	test_ibuf_basic();
	test_ibuf_shrink();
	test_ibuf_truncate();

	slab_cache_destroy(&cache);

	footer();
	return check_plan();
}
