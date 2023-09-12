#include <small/slab_cache.h>
#include <small/quota.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include "unit.h"

struct quota quota;
struct slab_arena arena;
struct slab_cache cache;

enum { NRUNS = 25, ITERATIONS = 1000, MAX_ALLOC = 5000000 };
static struct slab *runs[NRUNS];

static void
test_slab_cache(void)
{
	plan(1);
	header();

	slab_arena_create(&arena, &quota, 0, 4000000, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	int i = 0;

	while (i < ITERATIONS) {
		int run = random() % NRUNS;
		int size = random() % MAX_ALLOC;
		if (runs[run]) {
			slab_put(&cache, runs[run]);
		}
		runs[run] = slab_get(&cache, size);
		fail_unless(runs[run]);
		slab_cache_check(&cache);
		i++;
	}

	/* Put all allocated memory back to cache */
	for (i = 0; i < NRUNS; i++) {
		if (runs[i])
			slab_put(&cache, runs[i]);
	}
	slab_cache_check(&cache);

	/*
	 * It is allowed to hold only one slab of arena.
	 * If at lest one block was allocated then after freeing
	 * all memory it must be exactly one slab.
	 */
	ok(cache.allocated.stats.total == arena.slab_size);

	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);

	footer();
	check_plan();
}

static void
test_slab_real_size(void)
{
	plan(4);
	header();

	slab_arena_create(&arena, &quota, 0, 4000000, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	const size_t MB = 1024 * 1024;
	ok(slab_real_size(&cache, 0) == cache.order0_size);
	ok(slab_real_size(&cache, MB - slab_sizeof()) == MB);
	ok(slab_real_size(&cache, MB - slab_sizeof() + 1) == 2 * MB);
	ok(slab_real_size(&cache, 4564477 - slab_sizeof()) == 4564477);

	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);

	footer();
	check_plan();
}

int
main(void)
{
	plan(2);
	header();

	unsigned int seed = time(NULL);
	note("random seed is %u", seed);
	srand(seed);
	quota_init(&quota, UINT_MAX);

	test_slab_cache();
	test_slab_real_size();

	footer();
	return check_plan();
}
