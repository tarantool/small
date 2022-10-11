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
	header();

	struct region region;

	region_create(&region, &cache);

	fail_unless(region_used(&region) == 0);

	void *ptr = region_alloc(&region, 10);

	fail_unless(ptr);

	fail_unless(region_used(&region) == 10);

	ptr = region_alloc(&region, 10000000);
	fail_unless(ptr);

	fail_unless(region_used(&region) == 10000010);

	region_free(&region);

	fail_unless(region_used(&region) == 0);

	footer();
}

void
region_test_truncate()
{
	header();

	struct region region;

	region_create(&region, &cache);

	void *ptr = region_alloc(&region, 10);

	fail_unless(ptr);

	size_t used = region_used(&region);

	region_alloc(&region, 10000);
	region_alloc(&region, 10000000);

	region_truncate(&region, used);

	fail_unless(region_used(&region) == used);

	region_free(&region);

	footer();
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
	header();

	struct region region;
	struct region_cb_data data;

	region_create(&region, &cache);
	region_set_callbacks(&region,
			     region_on_alloc, region_on_truncate, &data);

	memset(&data, 0, sizeof(data));
	void *ptr = region_alloc(&region, 10);
	fail_unless(ptr);
	fail_unless(region_used(&region) == 10);
	fail_unless(data.region == &region);
	fail_unless(data.used == 0);
	fail_unless(data.value == 10);

	memset(&data, 0, sizeof(data));
	ptr = region_alloc(&region, 10000000);
	fail_unless(ptr);
	fail_unless(region_used(&region) == 10000010);
	fail_unless(data.region == &region);
	fail_unless(data.used == 10);
	fail_unless(data.value == 10000000);

	memset(&data, 0, sizeof(data));
	region_truncate(&region, 10);
	fail_unless(region_used(&region) == 10);
	fail_unless(data.region == &region);
	fail_unless(data.used == 10);
	fail_unless(data.value == 10);

	memset(&data, 0, sizeof(data));
	region_free(&region);
	fail_unless(region_used(&region) == 0);
	fail_unless(data.region == &region);
	fail_unless(data.used == 0);
	fail_unless(data.value == 0);

	region_reserve(&region, 100);
	memset(&data, 0, sizeof(data));
	ptr = region_alloc(&region, 1);
	fail_unless(ptr);
	fail_unless(region_used(&region) == 1);
	fail_unless(data.region == &region);
	fail_unless(data.used == 0);
	fail_unless(data.value == 1);

	memset(&data, 0, sizeof(data));
	ptr = region_aligned_alloc(&region, 32, 8);
	fail_unless(ptr);
	fail_unless(region_used(&region) == 40);
	fail_unless(data.region == &region);
	fail_unless(data.used == 1);
	fail_unless(data.value == 32 + 7);

	memset(&data, 0, sizeof(data));
	region_free(&region);
	fail_unless(region_used(&region) == 0);
	fail_unless(data.region == &region);
	fail_unless(data.used == 0);
	fail_unless(data.value == 0);

	footer();
}

void
region_test_poison()
{
	header();

#ifndef NDEBUG
	struct region region;
	region_create(&region, &cache);
	char pattern[100];
	memset(pattern, 'P', 100);

	region_reserve(&region, 100);
	void *ptr1 = region_alloc(&region, 10);
	memset(ptr1, 0, 10);
	fail_unless(ptr1 != NULL);
	void *ptr2 = region_alloc(&region, 90);
	memset(ptr2, 0, 90);
	fail_unless(ptr1 != NULL);

	region_truncate(&region, 10);
	fail_unless(memcmp(ptr2, pattern, 90) == 0);

	region_truncate(&region, 0);
	fail_unless(memcmp(ptr1, pattern, 10) == 0);
#endif

	footer();
}

int main()
{
	quota_init(&quota, UINT_MAX);
	slab_arena_create(&arena, &quota, 0,
			  4000000, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	region_basic();
	region_test_truncate();
	region_test_callbacks();
	region_test_poison();

	slab_cache_destroy(&cache);
}
