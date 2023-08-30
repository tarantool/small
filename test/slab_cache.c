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
	ok_no_asan(cache.allocated.stats.total == arena.slab_size);

	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);

	footer();
	check_plan();
}

#ifndef ENABLE_ASAN

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

#else

static char assert_msg_buf[128];

static void
on_assert_failure(const char *filename, int line, const char *funcname,
		  const char *expr)
{
	(void)filename;
	(void)line;
	snprintf(assert_msg_buf, sizeof(assert_msg_buf), "%s in %s",
		 expr, funcname);
	small_on_assert_failure = small_on_assert_failure_default;
}

static void
test_slab_membership(void)
{
	plan(1);
	header();

	struct slab_cache cache1;
	struct slab_cache cache2;

	slab_arena_create(&arena, &quota, 0, 4000000, MAP_PRIVATE);
	slab_cache_create(&cache1, &arena);
	slab_cache_create(&cache2, &arena);

	void *ptr = slab_get(&cache1, 1000);
	fail_unless(ptr != NULL);
	small_on_assert_failure = on_assert_failure;
	slab_put(&cache2, ptr);
	small_on_assert_failure = small_on_assert_failure_default;
	ok(strstr(assert_msg_buf,
		  "object and cache id mismatch\" in slab_put") != NULL);

	footer();
	check_plan();
}

#endif

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
#ifdef ENABLE_ASAN
	test_slab_membership();
#else
	test_slab_real_size();
#endif

	footer();
	return check_plan();
}
