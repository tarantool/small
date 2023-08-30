#include <small/region.h>
#include <small/quota.h>
#include <stdio.h>
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
	ok(region_used(&region) == 40);
	ok(data.region == &region);
	ok(data.used == 1);
	ok(data.value == 32 + 7);

	memset(&data, 0, sizeof(data));
	region_free(&region);
	ok(region_used(&region) == 0);
	ok(data.region == &region);
	ok(data.used == 0);
	ok(data.value == 0);

	footer();
	check_plan();
}

#ifndef NDEBUG

void
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

#endif

int main()
{
#ifndef NDEBUG
	plan(4);
#else
	plan(3);
#endif
	header();

	quota_init(&quota, UINT_MAX);
	slab_arena_create(&arena, &quota, 0,
			  4000000, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	region_basic();
	region_test_truncate();
	region_test_callbacks();
#ifndef NDEBUG
	region_test_poison();
#endif

	slab_cache_destroy(&cache);

	footer();
	return check_plan();
}
