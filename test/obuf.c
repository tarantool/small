#include <small/quota.h>
#include <small/obuf.h>
#include <small/slab_cache.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "unit.h"

enum {
	OBJSIZE_MIN = sizeof(int),
	OBJSIZE_MAX = 5000,
	OSCILLATION_MAX = 1024,
	ITERATIONS_MAX = 5000,
};

void
alloc_checked(struct obuf *buf)
{
	int size = OBJSIZE_MIN + rand() % (OBJSIZE_MAX - OBJSIZE_MIN + 1);
	fail_unless(size >= OBJSIZE_MIN && size <= OBJSIZE_MAX);
	obuf_alloc(buf, size);
}

static void
basic_alloc_streak(struct obuf *buf)
{
	for (int i = 0; i < OSCILLATION_MAX; ++i)
		alloc_checked(buf);
}

void
obuf_basic(struct slab_cache *slabc)
{
	int i;

	plan(3);
	header();

	struct obuf buf;
	obuf_create(&buf, slabc, 16320);
	ok(obuf_is_initialized(&buf));

	for (i = 0; i < ITERATIONS_MAX; i++) {
		basic_alloc_streak(&buf);
		fail_unless(obuf_capacity(&buf) > 0);
		obuf_reset(&buf);
		fail_unless(obuf_size(&buf) == 0);
	}
	obuf_destroy(&buf);
	ok(!obuf_is_initialized(&buf));
	ok(slab_cache_used(slabc) == 0);
	slab_cache_check(slabc);

	footer();
	check_plan();
}

int main()
{
	struct slab_cache cache;
	struct slab_arena arena;
	struct quota quota;

	plan(1);
	header();

	unsigned int seed = time(NULL);
	note("random seed is %u", seed);
	srand(seed);

	quota_init(&quota, UINT_MAX);

	slab_arena_create(&arena, &quota, 0, 4000000,
			  MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	obuf_basic(&cache);

	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);

	footer();
	return check_plan();
}
