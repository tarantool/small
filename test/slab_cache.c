#include <small/slab_cache.h>
#include <small/quota.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include "unit.h"


enum { NRUNS = 25, ITERATIONS = 1000, MAX_ALLOC = 5000000 };
static struct slab *runs[NRUNS];

int main()
{
	srand(time(0));

	struct quota quota;
	struct slab_arena arena;
	struct slab_cache cache;

	quota_init(&quota, UINT_MAX);

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

	fail_unless(arena.used + cache.quota.available + cache.quota.leased ==
		    quota_used(&quota));

	/* Put all allocated memory back to cache */
	for (i = 0; i < NRUNS; i++) {
		if (runs[i])
			slab_put(&cache, runs[i]);
	}
	slab_cache_check(&cache);

	fail_unless(arena.used + cache.quota.available + cache.quota.leased ==
		    quota_used(&quota));

	/*
	 * It is allowed to hold only one slab of arena.
	 * If at lest one block was allocated then after freeing
	 * all memory it must be exactly one slab.
	 */
	if (cache.allocated.stats.total != arena.slab_size) {
		fail("Slab cache returned memory to arena", "false");
	}

	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);
}
