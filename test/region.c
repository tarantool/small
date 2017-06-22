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

	printf("name of a new region: %s.\n", region_name(&region));

	region_set_name(&region, "region");

	printf("set new region name: %s.\n", region_name(&region));

	region_set_name(&region, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

	printf("region name is truncated: %s.\n", region_name(&region));

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

void
region_allocate_itself()
{
	header();
	plan(6);

	uint32_t used_mem = cache.allocated.stats.used;

	struct region *region_p;
	char *data;
	size_t data_size = 100;
	size_t aligned_data_size;

	{
		struct region region;
		region_create(&region, &cache);
		region_p = region_alloc_object(&region, struct region);
		aligned_data_size = region_used(&region);
		memcpy(region_p, &region, sizeof(region));
		data = (char *) region_alloc(region_p, data_size);
		is(region_used(region_p), aligned_data_size + data_size,
		   "user data and region object are allocated");
	}
	is(region_used(region_p), aligned_data_size + data_size,
	   "region_p correctly is created");

	struct region region_copy = *region_p;
	for (size_t i = 0; i < data_size; ++i)
		data[i] = (char)i;
	is(memcmp(&region_copy, region_p, sizeof(region_copy)), 0,
	   "the data change doesn't change the region object");

	size_t new_data_size = arena.slab_size - rslab_sizeof() - 1;
	char *new_data = region_alloc(region_p, new_data_size);
	for (size_t i = 0; i < new_data_size; ++i)
		new_data[i] = (char)(i + data_size);
	size_t ok = 0;
	for (ok = 0; ok < data_size; ++ok)
		if (data[ok] != (char)ok)
			break;
	is(ok, data_size, "old data was not changed");

	region_truncate(region_p, aligned_data_size);
	is(region_used(region_p), aligned_data_size, "truncate");
	region_destroy(region_p);

	is(cache.allocated.stats.used, used_mem, "all slabs are freed");
	check_plan();
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
	region_allocate_itself();

	slab_cache_destroy(&cache);
}
