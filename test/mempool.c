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

	plan(2);
	header();

	mempool_create(&pool, &cache, objsize);
	ok(mempool_is_initialized(&pool));

	for (i = 0; i < ITERATIONS_MAX; i++) {
		basic_alloc_streak();
		allocating = ! allocating;
	}

	mempool_destroy(&pool);
	ok(!mempool_is_initialized(&pool));

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
		void *ptr[32];
		for (int i = 0; i < (int)lengthof(ptr); i++) {
			ptr[i] = mempool_alloc(&pool);
			uintptr_t addr = (uintptr_t)ptr[i];
			if (addr % size)
				fail("aligment", "wrong");
		}
		for (int i = 0; i < (int)lengthof(ptr); i++)
			mempool_free(&pool, ptr[i]);
		mempool_destroy(&pool);
	}
	ok(true);

	footer();
	check_plan();
}

#ifdef ENABLE_ASAN

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
mempool_membership(void)
{
	plan(1);
	header();

	struct mempool pool1;
	struct mempool pool2;

	mempool_create(&pool1, &cache, 100);
	mempool_create(&pool2, &cache, 200);
	void *ptr = mempool_alloc(&pool1);
	fail_unless(ptr != NULL);
	small_on_assert_failure = on_assert_failure;
	mempool_free(&pool2, ptr);
	small_on_assert_failure = small_on_assert_failure_default;
	ok(strstr(assert_msg_buf,
		  "object and pool id mismatch\" in mempool_free") != NULL);

	footer();
	check_plan();
}

#endif /* ifdef ENABLE_ASAN */

int main()
{
#ifdef ENABLE_ASAN
	plan(3);
#else
	plan(2);
#endif
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
#ifdef ENABLE_ASAN
	mempool_membership();
#endif

	slab_cache_destroy(&cache);

	footer();
	return check_plan();
}
