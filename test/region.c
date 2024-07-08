#include <small/region.h>
#include <small/quota.h>
#include <stdio.h>
#include <time.h>
#include "unit.h"

struct slab_cache cache;
struct slab_arena arena;
struct quota quota;

void
region_basic()
{
	plan(6);
	header();

	struct region region;
	region_create(&region, &cache);
	ok(region_used(&region) == 0);

	void *ptr = region_alloc(&region, 10);
	ok(ptr != NULL);
	ok(region_used(&region) == 10);

	ptr = region_alloc(&region, 10000000);
	ok(ptr != NULL);
	ok(region_used(&region) == 10000010);

	region_free(&region);
	ok(region_used(&region) == 0);

	footer();
	check_plan();
}

void
region_test_truncate()
{
	plan(2);
	header();

	struct region region;
	region_create(&region, &cache);

	void *ptr = region_alloc(&region, 10);
	ok(ptr != NULL);

	size_t used = region_used(&region);
	region_alloc(&region, 10000);
	region_alloc(&region, 10000000);
	region_truncate(&region, used);
	ok(region_used(&region) == used);

	region_free(&region);

	footer();
	check_plan();
}

struct region_cb_data {
	struct region *region;
	size_t used;
	size_t value;
};

static void
region_on_alloc(struct region *region, size_t size, void *cb_arg)
{
	struct region_cb_data *cb_data = cb_arg;
	cb_data->region = region;
	cb_data->used = region_used(region);
	cb_data->value = size;
}

static void
region_on_truncate(struct region *region, size_t used, void *cb_arg)
{
	struct region_cb_data *cb_data = cb_arg;
	cb_data->region = region;
	cb_data->used = region_used(region);
	cb_data->value = used;
}

void
region_test_callbacks()
{
	plan(32);
	header();

	struct region region;
	struct region_cb_data data;

	region_create(&region, &cache);
	region_set_callbacks(&region,
			     region_on_alloc, region_on_truncate, &data);

	memset(&data, 0, sizeof(data));
	void *ptr = region_alloc(&region, 10);
	ok(ptr != NULL);
	ok(region_used(&region) == 10);
	ok(data.region == &region);
	ok(data.used == 0);
	ok(data.value == 10);

	memset(&data, 0, sizeof(data));
	ptr = region_alloc(&region, 10000000);
	ok(ptr != NULL);
	ok(region_used(&region) == 10000010);
	ok(data.region == &region);
	ok(data.used == 10);
	ok(data.value == 10000000);

	memset(&data, 0, sizeof(data));
	region_truncate(&region, 10);
	ok(region_used(&region) == 10);
	ok(data.region == &region);
	ok(data.used == 10);
	ok(data.value == 10);

	memset(&data, 0, sizeof(data));
	region_free(&region);
	ok(region_used(&region) == 0);
	ok(data.region == &region);
	ok(data.used == 0);
	ok(data.value == 0);

	region_reserve(&region, 100);
	memset(&data, 0, sizeof(data));
	ptr = region_alloc(&region, 1);
	ok(ptr != NULL);
	ok(region_used(&region) == 1);
	ok(data.region == &region);
	ok(data.used == 0);
	ok(data.value == 1);

	memset(&data, 0, sizeof(data));
	ptr = region_aligned_alloc(&region, 32, 8);
	ok(ptr != NULL);
#ifndef ENABLE_ASAN
	ok(region_used(&region) == 40);
#else
	ok(region_used(&region) == 33);
#endif
	ok(data.region == &region);
	ok(data.used == 1);
#ifndef ENABLE_ASAN
	ok(data.value == 32 + 7);
#else
	ok(data.value == 32);
#endif

	memset(&data, 0, sizeof(data));
	region_free(&region);
	ok(region_used(&region) == 0);
	ok(data.region == &region);
	ok(data.used == 0);
	ok(data.value == 0);

	footer();
	check_plan();
}

static void
region_test_join()
{
	plan(1);
	header();

	struct region region;
	region_create(&region, &cache);
	for (int i = 0; i < 217; i++) {
		size_t sizes[99];
		size_t total = 0;
		int n = 1 + rand() % 99;
		for (int j = 0; j < n; j++) {
			size_t size = 1 + rand() % 777;
			void *ptr = region_alloc(&region, size);
			fail_unless(ptr != NULL);
			memset(ptr, j + 1, size);
			sizes[j] = size;
			total += size;
		}
		char *ptr = region_join(&region, total);
		fail_unless(ptr != NULL);
		for (int j = 0; j < n; j++) {
			for (char *end = ptr + sizes[j]; ptr < end; ptr++)
				fail_unless(*ptr == j + 1);
		}
	}
	region_destroy(&region);
	ok(true);

	footer();
	check_plan();
}

static void
region_test_alignment()
{
	plan(1);
	header();

	struct region region;
	region_create(&region, &cache);
	size_t join_size = 0;
	for (int i = 0; i < 9999; i++) {
		int s = rand() % 7;
		if (s == 0) {
			size_t size = 1 + rand() % 333;
			void *ptr = region_alloc(&region, size);
			fail_unless(ptr != NULL);
			fail_unless_asan((uintptr_t)ptr % 2 != 0);
			join_size += size;
		} else if (s == 1) {
			if (join_size == 0)
				continue;
			void *ptr = region_join(&region, join_size);
			fail_unless(ptr != NULL);
			fail_unless_asan((uintptr_t)ptr % 2 != 0);
			join_size = 0;
		} else {
			size_t used = region_used(&region);
			size_t alignment = 2 << rand() % 5;
			void *ptr = region_aligned_alloc(&region,
							 1 + rand() % 333,
							 alignment);
			fail_unless(ptr != NULL);
			fail_unless((uintptr_t)ptr % alignment == 0);
			fail_unless_asan((uintptr_t)ptr % (alignment * 2) != 0);
			join_size += region_used(&region) - used;
		}
	}
	region_destroy(&region);
	ok(true);

	footer();
	check_plan();
}

#ifndef ENABLE_ASAN
#ifndef NDEBUG

static void
region_test_poison()
{
	plan(2);
	header();

	struct region region;
	region_create(&region, &cache);
	char pattern[100];
	memset(pattern, 'P', 100);

	region_reserve(&region, 100);
	void *ptr1 = region_alloc(&region, 10);
	fail_unless(ptr1 != NULL);
	memset(ptr1, 0, 10);
	void *ptr2 = region_alloc(&region, 90);
	fail_unless(ptr2 != NULL);
	memset(ptr2, 0, 90);

	region_truncate(&region, 10);
	ok(memcmp(ptr2, pattern, 90) == 0);

	region_truncate(&region, 0);
	ok(memcmp(ptr1, pattern, 10) == 0);

	footer();
	check_plan();
}

#endif /* ifndef NDEBUG */
#else /* ifdef ENABLE_ASAN */

static void
region_test_poison()
{
	plan(1);
	header();

	struct region region;
	region_create(&region, &cache);
	size_t size_max = 2 * small_getpagesize();
	for (int i = 0; i < 87; i++) {
		size_t size_r = 1 + rand() % size_max;
		size_t size_a = 1 + rand() % size_r;
		char *ptr_r = region_reserve(&region, size_r);
		fail_unless(ptr_r != NULL);
		memset(ptr_r, 0, size_a);
		char *ptr_a = region_alloc(&region, size_a);
		fail_unless(ptr_r == ptr_a);
		for (char *p = ptr_r + size_a; p < ptr_r + size_r; p++)
			fail_unless(__asan_address_is_poisoned(p));
	}
	region_destroy(&region);
	ok(true);

	footer();
	check_plan();
}

static void
region_test_tiny_reserve_size()
{
	plan(1);
	header();

	struct region region;
	region_create(&region, &cache);
	size_t size_max = SMALL_REGION_MIN_RESERVE;
	for (int i = 0; i < 87; i++) {
		size_t size = 1 + rand() % size_max;
		void *ptr = region_reserve(&region, size);
		fail_unless(ptr != NULL);
		memset(ptr, 0, size_max);
		region_alloc(&region, size);
	}
	region_destroy(&region);
	ok(true);

	footer();
	check_plan();
}

static void
region_test_truncate_reserved()
{
	plan(1);
	header();

	struct region region;
	region_create(&region, &cache);

	size_t used = region_used(&region);
	region_reserve(&region, 1024);
	region_truncate(&region, used);
	ok(rlist_empty(&region.allocations));
	region_destroy(&region);

	footer();
	check_plan();
}

#endif /* ifdef ENABLE_ASAN */

int main()
{
#ifdef ENABLE_ASAN
	plan(8);
#else
#ifndef NDEBUG
	plan(6);
#else
	plan(5);
#endif
#endif
	header();

	unsigned int seed = time(NULL);
	note("random seed is %u", seed);
	srand(seed);
	quota_init(&quota, UINT_MAX);
	slab_arena_create(&arena, &quota, 0,
			  4000000, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	region_basic();
	region_test_truncate();
	region_test_callbacks();
	region_test_join();
	region_test_alignment();
#ifndef ENABLE_ASAN
#ifndef NDEBUG
	region_test_poison();
#endif
#else
	region_test_poison();
	region_test_tiny_reserve_size();
	region_test_truncate_reserved();
#endif

	slab_cache_destroy(&cache);

	footer();
	return check_plan();
}
