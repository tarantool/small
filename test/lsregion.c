#include <small/lsregion.h>
#include <small/quota.h>
#include <stdio.h>
#include <string.h>
#include "unit.h"

struct quota quota;

enum { TEST_ARRAY_SIZE = 10 };

static uint32_t
rlist_size(struct rlist *head)
{
	struct rlist *next;
	uint32_t res = 0;
	rlist_foreach(next, head) {
		++res;
	}
	return res;
}

static void
slab_arena_print(struct slab_arena *arena)
{
	printf("#arena->prealloc = %zu\n#arena->maxalloc = %zu\n"
	       "#arena->used = %zu\n#arena->slab_size = %u\n",
	       arena->prealloc, quota_total(arena->quota),
	       arena->used, arena->slab_size);
}

/**
 * Test constructor, allocation and truncating of one memory
 * block.
 */
static void
test_basic()
{
	plan(15);
	header();

	struct slab_arena arena;
	struct lsregion allocator;
	fail_if(slab_arena_create(&arena, &quota, 0, 1024, MAP_PRIVATE) != 0);
	lsregion_create(&allocator, &arena);
	is(allocator.slab_size, arena.slab_size, "Slab size lsregion: %u, "\
	   "slab size arena: %u", allocator.slab_size, arena.slab_size);

	/* Test that initialization was correct. */

	fail_if(lsregion_used(&allocator) > 0);
	fail_if(lsregion_total(&allocator) > 0);
	slab_arena_print(&arena);

	/* Try to reserve and alloc 100 bytes. */

	uint32_t size = 100;
	int64_t id = 10;
	char *data = lsregion_reserve(&allocator, size, id);
	uint32_t used = lsregion_used(&allocator);
	uint32_t total = lsregion_total(&allocator);
	is(used, 0, "Used %u, must be used 0", used);
	is(total, arena.slab_size, "Total %u, must be total %u", total,
	   arena.slab_size);

	data = lsregion_alloc(&allocator, size, id);
	slab_arena_print(&arena);

	/* Used size is same that was allocated. */

	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, size, "Used %u, must be used %u", used, size);
	is(total, arena.slab_size, "Total %u, must be total %u", total,
	   arena.slab_size);

	/*
	 * Truncate with id < the allocated block id has't any
	 * effect.
	 */

	lsregion_gc(&allocator, id / 2);
	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, size, "Used %u, must be used %u", used, size);
	is(total, arena.slab_size, "Total %u, must be total %u", total,
	   arena.slab_size);

	uint32_t block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, 1, "Blocks count %u, must be 1", block_count);
	slab_arena_print(&arena);

	/*
	 * Tuncate the allocated block. Used bytes count is 0 now.
	 * But total = lsregion.slab_size, because the last slab
	 * is cached.
	 */

	lsregion_gc(&allocator, id);
	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, 0, "Used %u, must be used 0", used);
	is(total, arena.slab_size, "Total %u, must be total %u", total,
	   arena.slab_size);
	isnt(allocator.cached, NULL, "Cached is not NULL");
	slab_arena_print(&arena);

	/*
	 * Try to allocate block with size > specified
	 * slab_size without reserve.
	 */
	size = 2048;
	++id;
	data = lsregion_alloc(&allocator, size, id);
	fail_if(data == NULL);

	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, size, "Used %u, must be used %u", used, size);
	is(total, arena.slab_size, "Total %u, must be total %u", total,
	   arena.slab_size);
	is(allocator.cached, NULL, "Cached slab is used");

	block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, 1, "Blocks count %u, must be 1", block_count);
	slab_arena_print(&arena);

	lsregion_destroy(&allocator);
	slab_arena_print(&arena);
	slab_arena_destroy(&arena);

	footer();
	check_plan();
}

static void
fill_data(char **data, uint32_t count, uint32_t size, uint32_t start_id,
	  struct lsregion *allocator)
{
	for (uint32_t i = 0; i < count; ++i) {
		data[i] = lsregion_alloc(allocator, size, start_id++);
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

/** Test memory reserve. */
static void
test_reserve()
{
	plan(10);
	header();

	struct slab_arena arena;
	struct lsregion allocator;
	fail_if(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE) != 0);
	lsregion_create(&allocator, &arena);

	uint32_t size = arena.slab_size / 11;
	char *data[TEST_ARRAY_SIZE];
	uint32_t id = 0;

	/* Reserve memory for the entire array at once. */

	fail_if(lsregion_reserve(&allocator, size * TEST_ARRAY_SIZE,
				 id) == NULL);
	slab_arena_print(&arena);

	uint32_t used = lsregion_used(&allocator);
	uint32_t total = lsregion_total(&allocator);
	is(used, 0, "Used %u, must be used 0", used);
	is(total, arena.slab_size, "Total %u, must be total %u", total,
	   arena.slab_size);

	/* Allocate already reserved memory. */

	fill_data(data, TEST_ARRAY_SIZE, size, 0, &allocator);
	id += TEST_ARRAY_SIZE;

	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, size * TEST_ARRAY_SIZE, "Used %u, must be used %u", used,
	   size * TEST_ARRAY_SIZE);
	is(total, arena.slab_size, "Total %u, must be total %u", total,
	   arena.slab_size);
	slab_arena_print(&arena);

	/*
	 * Delete all data, and try to reserve a memory space in
	 * the not empty allocator.
	 */
	lsregion_gc(&allocator, id);
	++id;
	data[0] = lsregion_alloc(&allocator, size, id);
	++id;
	data[1] = lsregion_reserve(&allocator, size, id);

	used = lsregion_used(&allocator);
	total = lsregion_total(&allocator);
	is(used, size, "Used %u, must be used %u", used, size);
	is(total, arena.slab_size, "Total %u, must be total %u", total,
	   arena.slab_size);

	++id;
	data[2] = lsregion_alloc(&allocator, size, id);
	is(data[1], data[2], "Alloced on the same place that was reserved");

	/*
	 * Delete all data, and the following case:
	 * - allocate data in the first slab
	 *
	 * - reserve too much data to fit in the first slab, so
	 *       the second slab is reserved
	 *
	 * - allocate data that can fit in the first slab. In that
	 *       case the second slab still is not used and data
	 *       still can be allocated in the first slab.
	 *
	 *   @same lsregion_reserve_lslab() for details.
	 */
	lsregion_gc(&allocator, id);

	++id;
	size = arena.slab_size * 0.6;
	data[0] = lsregion_alloc(&allocator, size, id);

	char *data0_end = data[0] + size;

	++id;
	data[1] = lsregion_reserve(&allocator, arena.slab_size * 0.5, id);

	isnt(data[0], data[1], "Data allocted in first slab, and next data "\
	     "reserved in second slab");

	++id;
	data[2] = lsregion_alloc(&allocator, arena.slab_size * 0.1, id);
	is(data[2], data0_end, "Data must be alloced in previous block");

	++id;
	data[3] = lsregion_alloc(&allocator, arena.slab_size * 0.5, id);

	is(data[3], data[1], "Data must be alloced in next block that was "\
	   "reserved earlier.")

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	footer();
	check_plan();
}

/** Test many blocks allocation in one slab. */
static void
test_many_allocs_one_slab()
{
	plan(4);
	header();

	struct slab_arena arena;
	struct lsregion allocator;
	fail_if(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE) != 0);
	lsregion_create(&allocator, &arena);

	/*
	 * Allocate many small blocks that are fitting in one slab
	 * and fill them with simple data.
	 */
	const int count = TEST_ARRAY_SIZE;
	char *data[TEST_ARRAY_SIZE];
	uint32_t size = 400;
	fill_data(data, count, size, 0, &allocator);
	slab_arena_print(&arena);

	/*
	 * Used bytes count is count * size, but only one slab is
	 * used.
	 */
	uint32_t total_size = size * count;
	uint32_t used = lsregion_used(&allocator);
	is(used, total_size, "Total used %u, must be %u", used, total_size);

	uint32_t block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, 1, "Blocks count %u, must be 1", block_count);

	test_data(data, count, size);

	/*
	 * Try to truncate the middle of memory blocks, but it
	 * hasn't an effect since the lsregion allocator can't
	 * truncate a part of a slab.
	 */
	uint32_t middle_id = count / 2;
	lsregion_gc(&allocator, middle_id);

	used = lsregion_used(&allocator);;
	is(used, total_size, "Total used %u, must be %u", used, total_size);

	block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, 1, "Blocks count %u, must be 1", block_count);

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	footer();

	check_plan();
}

/** Test many memory blocks in many slabs. */
static void
test_many_allocs_many_slabs()
{
	plan(5);
	header();

	struct slab_arena arena;
	struct lsregion allocator;
	fail_if(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE) != 0);
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
	slab_arena_print(&arena);

	/*
	 * Used bytes count is count * size, but only one slab is
	 * used.
	 */

	uint32_t total_size = size * count;
	uint32_t used = lsregion_used(&allocator);
	is(used, total_size, "Total used %u, must be %u", used, total_size);

	uint32_t block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, 1, "Blocks count %u, must be 1", block_count);

	test_data(data, count, size);

	/* Allocate more memory blocks in a second slab. */

	char *next_block_data[count];
	fill_data(next_block_data, count, size, id, &allocator);
	id += count;
	total_size += size * count;
	used = lsregion_used(&allocator);
	slab_arena_print(&arena);

	/* Test that the first slab is still exists. */

	is(used, total_size, "Total used %u, must be %u", used, total_size);

	/* Truncate the first slab. */

	uint32_t block_max_id = count;
	lsregion_gc(&allocator, block_max_id);
	block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, 1, "Blocks count %u, must be 1", block_count);
	slab_arena_print(&arena);

	/* The second slab still has valid data. */

	test_data(next_block_data, count, size);

	/* Truncate the second slab. */

	block_max_id = id;
	lsregion_gc(&allocator, block_max_id);
	block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, 0, "Blocks count %u, must be 0", block_count);
	fail_if(lsregion_used(&allocator) > 0);

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	footer();
	check_plan();
}

/**
 * Test allocation of many big memory blocks, but specify a little
 * slab_size for the slab arena.
 */
static void
test_big_data_small_slabs()
{
	plan(3);
	header();

	struct slab_arena arena;
	struct lsregion allocator;
	fail_if(slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE) != 0);
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
	slab_arena_print(&arena);

	uint32_t block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, count, "Blocks count %u, must be %u", block_count,
	   count);

	id += count;
	uint32_t total_size = size * count;
	uint32_t used = lsregion_used(&allocator);
	is(used, total_size, "Total used %u, must be %u", used, total_size);

	/* Try to truncate a middle of the memory blocks. */

	lsregion_gc(&allocator, id / 2);
	block_count = rlist_size(&allocator.slabs.slabs);
	is(block_count, count / 2 - 1, "Blocks count %u, must be %u",
	   block_count, count / 2 - 1);
	slab_arena_print(&arena);

	fail_if(lsregion_used(&allocator) == 0);

	lsregion_gc(&allocator, id);
	fail_if(lsregion_used(&allocator) > 0);

	lsregion_destroy(&allocator);
	slab_arena_destroy(&arena);

	footer();
	check_plan();
}

int
main()
{
	quota_init(&quota, UINT_MAX);

	plan(5);

	test_basic();
	test_reserve();
	test_many_allocs_one_slab();
	test_many_allocs_many_slabs();
	test_big_data_small_slabs();

	return check_plan();
}
