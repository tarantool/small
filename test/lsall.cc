#include <small/lsall.h>
#include <small/slab_arena.h>
#include <small/quota.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "unit.h"

enum {
	OBJSIZE_MIN = 12,
	OBJSIZE_MAX = 5000,
	OBJECTS_MAX = 1000,
	OSCILLATION_MAX = 1024,
	ITERATIONS_MAX = 5000,
	MATRAS_CHUNCK_SIZE = 16 * 1024,
};

struct slab_arena arena;
struct lsall alloc;
struct quota quota;
/* Streak type - allocating or freeing */
bool allocating = true;
/** Keep global to easily inspect the core. */
long seed;

static uint32_t ids[OBJECTS_MAX];

static void *
mchank_alloc()
{
	return malloc(MATRAS_CHUNCK_SIZE);
}

static void
mchank_free(void *ptr)
{
	free(ptr);
}


static inline void
free_checked(uint32_t id)
{
	uint32_t *ptr = (uint32_t *)lsall_get(&alloc, id);

	fail_unless(ptr[1] == lsall_get_size(ptr));
	fail_unless(ptr[0] < OBJECTS_MAX && ids[ptr[0]] == id &&
		    ptr[ptr[1]/sizeof(int)-1] == ptr[0]);
	ids[ptr[0]] = UINT32_MAX;
	lsfree(&alloc, id);
}

static inline void
alloc_checked()
{
	int pos = rand() % OBJECTS_MAX;
	int size = rand() % OBJSIZE_MAX;
	if (size < OBJSIZE_MIN || size > OBJSIZE_MAX)
		size = OBJSIZE_MIN;

	if (ids[pos] != UINT32_MAX)
		free_checked(ids[pos]);

	if (!allocating)
		return;

	uint32_t *ptr = (uint32_t *)lsalloc(&alloc, size, ids + pos);
	ptr[0] = pos;
	ptr[1] = size;
	ptr[size/sizeof(int)-1] = pos;
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
small_alloc_basic()
{
	int i;
	header();

	lsall_create(&alloc, &arena, MATRAS_CHUNCK_SIZE,
		     mchank_alloc, mchank_free);

	for (i = 0; i < ITERATIONS_MAX; i++) {
		basic_alloc_streak();
		allocating = ! allocating;
	}

	lsall_destroy(&alloc);

	footer();
}

int main()
{
	seed = time(0);

	srand(seed);
	for (int i = 0; i < OBJECTS_MAX; i++)
		ids[i] = UINT32_MAX;

	quota_init(&quota, UINT_MAX);

	slab_arena_create(&arena, &quota, 0, 4000000,
			  MAP_PRIVATE);

	small_alloc_basic();


	slab_arena_destroy(&arena);
}
