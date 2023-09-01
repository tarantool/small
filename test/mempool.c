#include <small/mempool.h>
#include <small/quota.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "unit.h"

enum {
	OBJSIZE_MIN = 2 * sizeof(int),
	OBJSIZE_MAX = 4096,
	OBJECTS_MAX = 10000,
	OSCILLATION_MAX = 1024,
	ITERATIONS_MAX = 500,
};

struct slab_arena arena;
struct slab_cache cache;
struct quota quota;
struct mempool pool;
int objsize;
size_t used;
/* Streak type - allocating or freeing */
bool allocating = true;
/** Keep global to easily inspect the core. */
unsigned int seed;

static int *ptrs[OBJECTS_MAX];

static inline void
free_checked(int *ptr)
{
	fail_unless(ptr[0] < OBJECTS_MAX &&
		    ptr[objsize/sizeof(int)-1] == ptr[0]);
	int pos = ptr[0];
	fail_unless(ptrs[pos] == ptr);
	fail_unless(mempool_used(&pool) == used);
	ptrs[pos][0] = ptrs[pos][objsize/sizeof(int)-1] = INT_MAX;
	mempool_free(&pool, ptrs[pos]);
	ptrs[pos] = NULL;
	used -= objsize;
}

static inline void *
alloc_checked()
{
	int pos = rand() % OBJECTS_MAX;
	if (ptrs[pos]) {
		assert(ptrs[pos][0] == pos);
		free_checked(ptrs[pos]);
		ptrs[pos] = 0;
	}
	if (! allocating)
		return NULL;
	fail_unless(mempool_used(&pool) == used);
	used += objsize;
	ptrs[pos] = mempool_alloc(&pool);
	ptrs[pos][0] = pos;
	ptrs[pos][objsize/sizeof(int)-1] = pos;
	return ptrs[pos];
}


static void
basic_alloc_streak()
{
	int oscillation = rand() % OSCILLATION_MAX;
	int i;

	for (i = 0; i < oscillation; ++i) {
		alloc_checked();
	}
}

void
mempool_basic()
{
	int i;

	plan(1);
	header();

	mempool_create(&pool, &cache, objsize);

	for (i = 0; i < ITERATIONS_MAX; i++) {
		basic_alloc_streak();
		allocating = ! allocating;
	}
	ok(true);

	mempool_destroy(&pool);

	footer();
	check_plan();
}

void
mempool_align()
{
	plan(1);
	header();

	for (uint32_t size = OBJSIZE_MIN; size < OBJSIZE_MAX; size <<= 1) {
		mempool_create(&pool, &cache, size);
		for (uint32_t i = 0; i < 32; i++) {
			void *ptr = mempool_alloc(&pool);
			uintptr_t addr = (uintptr_t)ptr;
			if (addr % size)
				fail("aligment", "wrong");
		}
		mempool_destroy(&pool);
	}
	ok(true);

	footer();
	check_plan();
}

int main()
{
	plan(2);
	header();

	seed = time(NULL);
	note("random seed is %u", seed);
	srand(seed);

	objsize = rand() % OBJSIZE_MAX;
	if (objsize < OBJSIZE_MIN)
		objsize = OBJSIZE_MIN;
	/*
	 * Mempool does not work with not aligned sizes. Because
	 * it utilizes the unused blocks for storing internal
	 * info, which needs alignment.
	 */
	objsize = small_align(objsize, alignof(uint64_t));

	quota_init(&quota, UINT_MAX);

	slab_arena_create(&arena, &quota, 0,
			  4000000, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	mempool_basic();
	mempool_align();

	slab_cache_destroy(&cache);

	footer();
	return check_plan();
}
