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

/**
 * Make sure that in case cut_size == slab_used (i.e. we are going
 * to truncate whole slab) slab itself is not put back to the slab
 * cache. It should be kept in the list and reused instead.
 */
void
region_test_truncate_rotate()
{
	header();

	struct region region;

	region_create(&region, &cache);

	size_t alloc1_sz = 10;
	void *ptr = region_alloc(&region, alloc1_sz);

	fail_unless(ptr != NULL);

	fail_if(region_total(&region) > 5000);
	size_t total_before_alloc = region_total(&region);

	/*
	 * This allocation does not fit in previously
	 * allocated slab (default slab size is 4kb), so
	 * there's two slabs now in the region list.
	 */
	size_t alloc2_sz = 10000;
	region_alloc(&region, alloc2_sz);
	fail_if(total_before_alloc == region_total(&region));

	/*
	 * Before truncate ('x' is occupied space):
	 *
	 *    1 slab      2 slab/HEAD
	 *   10b used      10kb used
	 *  +---------+   +---------+
	 *  |xx|      |-->|xxxxxx|  |--> NULL
	 *  |xx|      |   |xxxxxx|  |
	 *  +---------+   +---------+
	 *
	 * After truncate:
	 *
	 *    1 slab      2 slab/HEAD
	 *   10b used       0kb used
	 *  +---------+   +---------+
	 *  |xx|      |-->|         |--> NULL
	 *  |xx|      |   |         |
	 *  +---------+   +---------+
	 *
	 */
	size_t total_before_truncate = region_total(&region);
	region_truncate(&region, alloc1_sz);

	/*
	 * Whole slab has been purified but it shouldn't be
	 * returned back to the slab cache. So that it can be
	 * reused for further allocations.
	 */
	fail_if(total_before_truncate != region_total(&region));
	region_alloc(&region, alloc2_sz);
	fail_if(total_before_truncate != region_total(&region));

	region_free(&region);

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
	region_test_truncate_rotate();

	slab_cache_destroy(&cache);
}
