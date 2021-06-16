#include <small/small.h>
#include <small/quota.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "unit.h"

enum {
	GRANULARITY_MIN = 4,
	GRANULARITY_MAX = 1024,
	ALLOCATION_COUNT = 10,
};

struct slab_arena arena;
struct slab_cache cache;
struct small_alloc alloc;
struct quota quota;

static unsigned *ptrs[SMALL_MEMPOOL_MAX];

static inline void
free_checked(struct mempool *pool, unsigned *ptr)
{
	/*
	 * Checking previous saved data.
	 */
	fail_unless(ptr[0] < SMALL_MEMPOOL_MAX &&
		    ptr[pool->objsize/sizeof(unsigned)-1] == ptr[0]);
	int pos = ptr[0];
	fail_unless(ptrs[pos] == ptr);
	ptrs[pos][0] = ptrs[pos][pool->objsize/sizeof(unsigned)-1] = INT_MAX;
	mempool_free(pool, ptrs[pos]);
	ptrs[pos] = NULL;
}

static inline void *
alloc_checked(struct mempool *pool, unsigned pos)
{
	fail_unless(ptrs[pos] == NULL);
	ptrs[pos] = mempool_alloc(pool);
	fail_unless(ptrs[pos] != NULL);
	/*
	 * Saving some data for test purposes
	 */
	ptrs[pos][0] = pos;
	ptrs[pos][pool->objsize/sizeof(unsigned)-1] = pos;
	return ptrs[pos];
}

static void
small_granularity_aligment_test(void)
{
	header();
	const float alloc_factor = 1.3;
	for(unsigned int granularity = GRANULARITY_MIN;
	    granularity <= GRANULARITY_MAX;
	    granularity <<= 1) {
		float actual_alloc_factor;
		/*
		 * Creates small_alloc with different granularity,
		 * which must be power of two.
		 */
		small_alloc_create(&alloc, &cache, granularity,
			   granularity, alloc_factor, &actual_alloc_factor);
		/*
		 * Checks aligment of all mempools in small alloc.
		 */
		for (unsigned int mempool = 0;
		     mempool < alloc.small_mempool_cache_size;
		     mempool++) {
			/*
			 * Allocates memory in each of mempools for
			 * several objects. Each object must have at
			 * least granularity alignment
			 */
			struct mempool *pool =
				&alloc.small_mempool_cache[mempool].pool;
			for (unsigned cnt = 0; cnt < ALLOCATION_COUNT; cnt++) {
				ptrs[cnt] = mempool_alloc(pool);
				uintptr_t addr = (uintptr_t)ptrs[cnt];
				fail_unless ((addr % granularity) == 0);
			}
			/*
			 * Frees previous allocated objects.
			 */
			for (unsigned cnt = 0; cnt < ALLOCATION_COUNT; cnt++) {
				mempool_free(pool, ptrs[cnt]);
				ptrs[cnt] = NULL;
			}
		}
		small_alloc_destroy(&alloc);
	}
	footer();
}

static void
small_granularity_allocation_test(void)
{
	header();
	const float alloc_factor = 1.3;
	for(unsigned int granularity = GRANULARITY_MIN;
	    granularity <= GRANULARITY_MAX;
	    granularity <<= 1) {
		float actual_alloc_factor;
		/*
		 * Creates small_alloc with different granularity,
		 * which must be power of two.
		 */
		small_alloc_create(&alloc, &cache, granularity,
			   granularity, alloc_factor, &actual_alloc_factor);
		/*
		 * Checks allocation of all mempools in small_alloc
		 */
		for (unsigned int mempool = 0;
		     mempool < alloc.small_mempool_cache_size;
		     mempool++) {
			struct mempool *pool =
				&alloc.small_mempool_cache[mempool].pool;
			ptrs[mempool] = alloc_checked(pool, mempool);
		}
		for (unsigned int mempool = 0;
		     mempool < alloc.small_mempool_cache_size;
		     mempool++) {
			struct mempool *pool =
				&alloc.small_mempool_cache[mempool].pool;
			free_checked(pool, ptrs[mempool]);
			fail_unless(mempool_used(pool) == 0);
		}
		small_alloc_destroy(&alloc);
	}
	footer();
}

int main()
{
	quota_init(&quota, UINT_MAX);

	slab_arena_create(&arena, &quota, 0, 4000000,
			  MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	small_granularity_aligment_test();
	small_granularity_allocation_test();

	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);
}
