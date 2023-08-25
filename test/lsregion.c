#include <small/lsregion.h>
#include <small/quota.h>
#include <stdio.h>
#include <string.h>
#include "unit.h"


/* For the sake of lsregion_alloc_object (it demands alignof()) */
#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif

#if !defined(alignof) && !defined(__alignof_is_defined)
#  if __has_feature(c_alignof) || (defined(__GNUC__) && __GNUC__ >= 5)
#    include <stdalign.h>
#  elif defined(__GNUC__)
#    define alignof(_T) __alignof(_T)
#    define __alignof_is_defined 1
#  else
#    define alignof(_T) offsetof(struct { char c; _T member; }, member)
#    define __alignof_is_defined 1
#  endif
#endif

enum { TEST_ARRAY_SIZE = 10 };

static size_t
lsregion_slab_count(struct lsregion *region)
{
	size_t res = 0;
	struct rlist *next;
	rlist_foreach(next, &region->slabs.slabs)
		++res;
	return res;
}

/**
 * Test constructor, allocation and truncating of one memory
 * block.
 */
static void
test_basic()
{
	note("basic");
	plan(45);

	struct quota quota;
	struct slab_arena arena;
	struct lsregion allocator;
	quota_init(&quota, 4 * SLAB_MIN_SIZE);
	is(slab_arena_create(&arena, &quota, 0, 1024, MAP_PRIVATE), 0, "init");
	lsregion_create(&allocator, &arena);

	/* Test that initialization was correct. */
	is(lsregion_used(&allocator), 0, "used after init");
	is(lsregion_total(&allocator), 0, "total after init");
	is(arena.used, 0, "arena used after init");
	is(lsregion_slab_count(&allocator), 0, "slab count after init");
	is(allocator.cached, NULL, "slab cache after init");

	/* Try to alloc 100 bytes. */
	uint32_t size = 100;
	int64_t id = 10;
	char *data = lsregion_alloc(&allocator, size, id);
	isnt(data, NULL, "alloc(100)");
	uint32_t used = lsregion_used(&allocator);
	uint32_t total = lsregion_total(&allocator);
	is(used, size, "used after alloc(100)");
	is(total, arena.slab_size, "total after alloc(100)");
	is(arena.used, arena.slab_size, "arena used after alloc(100)");
	is(lsregion_slab_count(&allocator), 1, "slab count after alloc(100)");
	is(allocator.cached, NULL, "slab cache after alloc(100)");

	/*
	 * Truncate with id < the allocated block id has't any
	 * effect.
	 */
	lsregion_gc(&allocator, id / 2);
	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, size, "used after gc(id / 2)");
	is(total, arena.slab_size, "total after gc(id / 2)");
	is(arena.used, arena.slab_size, "arena used after gc(id / 2)");
	is(lsregion_slab_count(&allocator), 1, "slab count after gc(id / 2)");
	is(allocator.cached, NULL, "slab cache after gc(id / 2)");

	/*
	 * Tuncate the allocated block. Used bytes count is 0 now.
	 * But total = lsregion.slab_size, because the last slab
	 * is cached.
	 */
	lsregion_gc(&allocator, id);
	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, 0, "used after gc(id)");
	is(total, arena.slab_size, "total after gc(id)");
	is(arena.used, arena.slab_size, "arena used after gc(id)");
	is(lsregion_slab_count(&allocator), 0, "slab count after gc(id)");
	isnt(allocator.cached, NULL, "slab cache after gc(id)");

	/* Try alloc_object. */
	++id;
	isnt(lsregion_alloc(&allocator, 13, id), NULL, "unaligned allocation");
	struct foo {
		int a;
		double b;
	};
	void *ptr;
	++id;
	isnt(ptr = lsregion_alloc_object(&allocator, id, struct foo), NULL,
	     "alloc_object");
	is((uintptr_t)ptr % alignof(struct foo), 0, "alloc_object is correctly aligned");
	lsregion_gc(&allocator, id);

	/*
	 * Try to allocate block with size > specified slab_size.
	 */
	size = 2048;
	++id;
	data = lsregion_alloc(&allocator, size, id);
	isnt(data, NULL, "alloc(2048)");
	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, size, "used after alloc(2048)");
	is(total, arena.slab_size, "total after alloc(2048)");
	is(arena.used, arena.slab_size, "arena used after alloc(2048)");
	is(lsregion_slab_count(&allocator), 1, "slab count after alloc(2048)");
	is(allocator.cached, NULL, "slab cache after alloc(2048)");

	/*
	 * Large allocation backed by malloc()
	 */
	++id;
	size_t qused = quota_used(arena.quota);
	size_t aused = arena.used;
	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	size = arena.slab_size + 100;
	data = lsregion_alloc(&allocator, size, id);
	isnt(data, NULL, "large alloc()");
	is(lsregion_used(&allocator), used + size, "used after large alloc()");
	is(lsregion_total(&allocator), total + size + lslab_sizeof(),
	   "total after large alloc()");
	is(arena.used, aused, "arena used is not changed after large alloc()");
	size_t size_quota = (size + lslab_sizeof() + QUOTA_UNIT_SIZE - 1) &
				~(size_t)(QUOTA_UNIT_SIZE - 1);
	is(quota_used(arena.quota), qused + size_quota,
	   "quota used after large alloc()");
	is(lsregion_slab_count(&allocator), 2,
	   "slab count after large alloc()");
	is(allocator.cached, NULL, "slab cache after large alloc()");

	/*
	 * Allocation after large slab
	 */
	++id;
	size = 10;
	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	data = lsregion_alloc(&allocator, size, id);
	isnt(data, NULL, "alloc after large");
	is(lsregion_used(&allocator), used + size,
	   "alloc after large");
	is(lsregion_total(&allocator), total + arena.slab_size,
	   "large slab is not re-used");
	is(lsregion_slab_count(&allocator), 3, "large slab is not reused");

	/*
	 * gc of large slab
	 */
	lsregion_gc(&allocator, id);
	is(lsregion_slab_count(&allocator), 0,
	   "slab count after large gc()");

	/*
	 * Allocate exactly slab size.
	 */
	++id;
	data = lsregion_alloc(&allocator, arena.slab_size - lslab_sizeof(), id);
	lsregion_gc(&allocator, id);

	lsregion_destroy(&allocator);
	/* Sic: slabs are cached by arena */
	is(arena.used, 2 * arena.slab_size, "arena used after destroy");
	is(quota_used(arena.quota), 2 * arena.slab_size,
	   "quota used after destroy");
	slab_arena_destroy(&arena);

	check_plan();
}

static void
fill_data(char **data, uint32_t count, uint32_t size, uint32_t start_id,
	  struct lsregion *allocator)
{
	for (uint32_t i = 0; i < count; ++i) {
		data[i] = lsregion_alloc(allocator, size, start_id++);
		assert(data[i] != NULL);
		memset(data[i], i % CHAR_MAX, size);
	}
}

static void
test_data(char **data, uint32_t count, uint32_t size)
{
	for (uint32_t i = 0; i < count; ++i) {
		for (uint32_t j = 0; j < size; ++j) {
			fail_if(data[i][j] != (char) (i % CHAR_MAX));
		}
	}
}

/** Test many blocks allocation in one slab. */
static void
test_many_allocs_one_slab()
{
	note("many_allocs_one_slab");
	plan(6);

	struct quota quota;
	struct slab_arena arena;
	struct lsregion allocator;
	quota_init(&quota, 4 * SLAB_MIN_SIZE);
	is(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE), 0, "init");
	lsregion_create(&allocator, &arena);

	/*
	 * Allocate many small blocks that are fitting in one slab
	 * and fill them with simple data.
	 */
	const int count = TEST_ARRAY_SIZE;
	char *data[TEST_ARRAY_SIZE];
	uint32_t size = 400;
	fill_data(data, count, size, 0, &allocator);
	is(arena.used, arena.slab_size, "arena used after many small blocks");

	/*
	 * Used bytes count is count * size, but only one slab is
	 * used.
	 */
	uint32_t total_size = size * count;
	uint32_t used = lsregion_used(&allocator);
	is(used, total_size, "used after small blocks");
	is(lsregion_slab_count(&allocator), 1, "slab count after small blocks");

	test_data(data, count, size);

	/*
	 * Try to truncate the middle of memory blocks, but it
	 * hasn't an effect since the lsregion allocator can't
	 * truncate a part of a slab.
	 */
	uint32_t middle_id = count / 2;
	lsregion_gc(&allocator, middle_id);

	used = lsregion_used(&allocator);;
	is(used, total_size, "used after gc");
	is(lsregion_slab_count(&allocator), 1, "slab count after gc(id/2)");

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	check_plan();
}

/** Test many memory blocks in many slabs. */
static void
test_many_allocs_many_slabs()
{
	note("many_allocs_many_slabs");
	plan(10);

	struct quota quota;
	struct slab_arena arena;
	struct lsregion allocator;
	quota_init(&quota, 4 * SLAB_MIN_SIZE);
	is(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE), 0, "init");
	lsregion_create(&allocator, &arena);

	/*
	 * Allocate many small blocks that are fitting in one slab
	 * and fill them with simple data.
	 */
	const int count = TEST_ARRAY_SIZE + 1;
	char *data[TEST_ARRAY_SIZE + 1];
	uint32_t size = arena.slab_size / 12;
	uint32_t id = 0;
	fill_data(data, count, size, id, &allocator);
	id += count;
	is(arena.used, arena.slab_size, "arena used after one slab");

	/*
	 * Used bytes count is count * size, but only one slab is
	 * used.
	 */

	uint32_t total_size = size * count;
	uint32_t used = lsregion_used(&allocator);
	is(used, total_size, "used after one slab");
	is(lsregion_slab_count(&allocator), 1, "slab count after one slab");

	test_data(data, count, size);

	/* Allocate more memory blocks in a second slab. */

	char *next_block_data[count];
	fill_data(next_block_data, count, size, id, &allocator);
	id += count;
	total_size += size * count;
	used = lsregion_used(&allocator);
	is(arena.used, 2 * arena.slab_size, "arena used after many slabs");

	/* Test that the first slab is still exists. */

	is(used, total_size, "used after many slabs");

	/* Truncate the first slab. */

	uint32_t block_max_id = count;
	lsregion_gc(&allocator, block_max_id);
	is(lsregion_slab_count(&allocator), 1, "slab count after gc first");
	is(arena.used, 2 * arena.slab_size, "arena used after gc first");

	/* The second slab still has valid data. */

	test_data(next_block_data, count, size);

	/* Truncate the second slab. */

	block_max_id = id;
	lsregion_gc(&allocator, block_max_id);
	is(lsregion_slab_count(&allocator), 0, "slab count after gc second");
	is(arena.used, 2 * arena.slab_size, "arena used after gc second");
	fail_if(lsregion_used(&allocator) > 0);

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	check_plan();
}

/**
 * Test allocation of many big memory blocks, but specify a little
 * slab_size for the slab arena.
 */
static void
test_big_data_small_slabs()
{
	note("big_data_small_slabs");
	plan(7);

	struct quota quota;
	struct slab_arena arena;
	struct lsregion allocator;
	quota_init(&quota, 16 * SLAB_MIN_SIZE);
	is(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE), 0, "init");
	lsregion_create(&allocator, &arena);

	const uint32_t count = TEST_ARRAY_SIZE;
	char *data[TEST_ARRAY_SIZE];
	uint32_t size = arena.slab_size * 3 / 4;
	int64_t id = 0;

	/*
	 * Allocate big memory blocks and fill them with simple
	 * data.
	 */
	fill_data(data, count, size, id, &allocator);
	uint32_t total_size = size * count;
	uint32_t used = lsregion_used(&allocator);
	is(used, total_size, "used after alloc");
	is(arena.used, count * arena.slab_size, "arena used after alloc");
	is(lsregion_slab_count(&allocator), count, "slab count after alloc");

	id += count;

	/* Try to truncate a middle of the memory blocks. */
	lsregion_gc(&allocator, id / 2);
	isnt(lsregion_used(&allocator), 0, "used after gc(id / 2)");
	is(lsregion_slab_count(&allocator), count / 2 -1,
	   "slab count after gc (id / 2)");
	is(arena.used, count * arena.slab_size, "arena used after gc(id / 2)");

	lsregion_gc(&allocator, id);
	fail_if(lsregion_used(&allocator) > 0);

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	check_plan();
}

static void
test_reserve(void)
{
	header();
	plan(10);

	struct quota quota;
	struct slab_arena arena;
	struct lsregion allocator;
	quota_init(&quota, 16 * SLAB_MIN_SIZE);
	is(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE), 0, "init");
	lsregion_create(&allocator, &arena);

	void *p1 = lsregion_reserve(&allocator, 100);
	is(lsregion_used(&allocator), 0, "reserve does not occupy memory");
	is(lsregion_total(&allocator), arena.slab_size, "reserve creates slabs");
	void *p2 = lsregion_alloc(&allocator, 80, 1);
	is(p1, p2, "alloc returns the same as reserve, even if size is less");
	is(lsregion_used(&allocator), 80, "alloc updated 'used'");

	p1 = lsregion_reserve(&allocator, arena.slab_size - lslab_sizeof());
	is(lsregion_used(&allocator), 80, "next reserve didn't touch 'used'");
	is(lsregion_total(&allocator), arena.slab_size * 2, "but changed "
	   "'total' because second slab is allocated");
	is(lsregion_slab_count(&allocator), 2, "slab count is 2 now");
	lsregion_gc(&allocator, 1);

	is(lsregion_used(&allocator), 0, "gc works fine with empty reserved "
	   "slabs");
	is(lsregion_slab_count(&allocator), 0, "all slabs are removed");

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	check_plan();
	footer();
}

static void
test_aligned(void)
{
	header();
	plan(12);

	struct quota quota;
	struct slab_arena arena;
	struct lsregion allocator;
	quota_init(&quota, 16 * SLAB_MIN_SIZE);
	is(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE), 0, "init");
	lsregion_create(&allocator, &arena);
	int id = 0;

	++id;
	void *p1 = lsregion_aligned_alloc(&allocator, 8, 8, id);
	ok((unsigned long)p1 % 8 == 0, "trivial aligned");
	is(lsregion_used(&allocator), 8, "'used'");

	++id;
	void *p2 = lsregion_aligned_alloc(&allocator, 1, 16, id);
	ok((unsigned long)p2 % 16 == 0, "16 byte alignment for 1 byte");
	void *p3 = lsregion_aligned_alloc(&allocator, 1, 16, id);
	ok(p3 == (char *)p2 + 16, "second 16 aligned alloc of 1 byte is far "
	   "from first");
	ok((unsigned long)p3 % 16 == 0, "aligned by 16 too");

	is(lsregion_used(&allocator), (size_t)(p3 + 1 - p1), "'used'");

	++id;
	void *p4 = lsregion_aligned_alloc(&allocator, 3, 4, id);
	ok(p4 == (char *)p3 + 4, "align next by 4 bytes, should be closer to "
	   "previous");
	ok((unsigned long)p4 % 4 == 0, "aligned by 4");
	is(lsregion_used(&allocator), (size_t)(p4 + 3 - p1), "'used'");

	lsregion_gc(&allocator, id);

	++id;
	p1 = lsregion_aligned_alloc(&allocator,
				    arena.slab_size - lslab_sizeof(), 32, id);
	ok((unsigned long)p1 % 32 == 0, "32 byte aligned alloc of slab size");

	lsregion_gc(&allocator, id);
	is(lsregion_used(&allocator), 0, "gc deleted all");

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	check_plan();
	footer();
}

int
main()
{
	plan(6);

	test_basic();
	test_many_allocs_one_slab();
	test_many_allocs_many_slabs();
	test_big_data_small_slabs();
	test_reserve();
	test_aligned();

	return check_plan();
}
