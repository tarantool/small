#include <small/small.h>
#include <small/quota.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "unit.h"

enum {
	OBJSIZE_MIN = 3 * sizeof(int),
	OBJECTS_MAX = 1000
};

struct slab_arena arena;
struct slab_cache cache;
struct small_alloc alloc;
struct quota quota;
/* Streak type - allocating or freeing */
bool allocating = true;
/** Keep global to easily inspect the core. */
long seed;

static int *ptrs[OBJECTS_MAX];

static inline void
free_checked(int *ptr)
{
	fail_unless(ptr[0] < OBJECTS_MAX &&
		    ptr[ptr[1]/sizeof(int)-1] == ptr[0]);
	int pos = ptr[0];
	fail_unless(ptrs[pos] == ptr);
	ptrs[pos][0] = ptrs[pos][ptr[1]/sizeof(int)-1] = INT_MAX;
	smfree(&alloc, ptrs[pos], ptrs[pos][1]);
	ptrs[pos] = NULL;
}

static inline void *
alloc_checked(int pos, int size_min, int size_max)
{
	assert(size_max > size_min);
	int size = size_min + rand() % (size_max - size_min);

	if (ptrs[pos]) {
		assert(ptrs[pos][0] == pos);
		free_checked(ptrs[pos]);
	}
	if (! allocating)
		return NULL;
	ptrs[pos] = smalloc(&alloc, size);
	ptrs[pos][0] = pos;
	ptrs[pos][1] = size;
	ptrs[pos][size/sizeof(int)-1] = pos;
//	printf("size: %d\n", size);
	return ptrs[pos];
}

static int
small_is_unused_cb(const void *stats, void *arg)
{
	const struct mempool_stats *mempool_stats =
		(const struct mempool_stats *)stats;
	unsigned long *slab_total = arg;
	*slab_total += mempool_stats->slabsize * mempool_stats->slabcount;
	return 0;
}

static bool
small_is_unused(void)
{
	struct small_stats totals;
	unsigned long slab_total = 0;
	small_stats(&alloc, &totals, small_is_unused_cb, &slab_total);
	if (totals.used > 0)
		return false;
	if (slab_cache_used(&cache) > slab_total)
		return false;
	return true;
}

static void
small_alloc_test(int size_min, int size_max, int objects_max,
		 int oscillation_max, int iterations_max)
{
	float actual_alloc_factor;
	small_alloc_create(&alloc, &cache, OBJSIZE_MIN,
			   sizeof(intptr_t), 1.3,
			   &actual_alloc_factor);

	for (int i = 0; i < iterations_max; i++) {
		int oscillation = rand() % oscillation_max;
		for (int j = 0; j < oscillation; ++j) {
			int pos = rand() % objects_max;
			alloc_checked(pos, size_min, size_max);
		}
		allocating = ! allocating;
	}

	for (int pos = 0; pos < OBJECTS_MAX; pos++) {
		if (ptrs[pos] != NULL)
			free_checked(ptrs[pos]);
	}

	fail_unless(small_is_unused());

	small_alloc_destroy(&alloc);
}

static void
small_alloc_basic(void)
{
	plan(1);
	header();

	small_alloc_test(OBJSIZE_MIN, 5000, 1000, 1024, 5000);
	ok(true);

	footer();
	check_plan();
}

static void
small_alloc_large(void)
{
	plan(1);
	header();

	size_t large_size_min = mempool_objsize_max(cache.arena->slab_size);
	size_t large_size_max = 2 * cache.arena->slab_size;
	small_alloc_test(large_size_min, large_size_max, 50, 10, 100);
	ok(true);

	footer();
	check_plan();
}

int main()
{
	plan(2);
	header();

	seed = time(0);
	srand(seed);

	quota_init(&quota, UINT_MAX);
	slab_arena_create(&arena, &quota, 0, 4000000,
			  MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	small_alloc_basic();
	small_alloc_large();

	slab_cache_destroy(&cache);

	footer();
	return check_plan();
}
