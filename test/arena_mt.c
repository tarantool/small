#include <small/slab_arena.h>
#include <small/quota.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif
#include "unit.h"

struct slab_arena arena;
struct quota quota;

int THREADS = 8;
int ITERATIONS = 1009 /* 100003 */;
int OSCILLATION = 137;
int FILL = SLAB_MIN_SIZE/sizeof(pthread_t);

void *
run(void *p __attribute__((unused)))
{
#ifdef __FreeBSD__
	unsigned int seed = pthread_getthreadid_np();
#else
	unsigned int seed = (intptr_t) pthread_self();
#endif
	note("random seed is %u", seed);
	int iterations = rand_r(&seed) % ITERATIONS;
	pthread_t **slabs = slab_map(&arena);
	for (int i = 0; i < iterations; i++) {
		int oscillation = rand_r(&seed) % OSCILLATION;
		for (int osc = 0; osc  < oscillation; osc++) {
			slabs[osc] = (pthread_t *) slab_map(&arena);
			for (int fill = 0; fill < FILL; fill += 100) {
				slabs[osc][fill] = pthread_self();
			}
		}
		sched_yield();
		for (int osc = 0; osc  < oscillation; osc++) {
			for (int fill = 0; fill < FILL; fill+= 100) {
				fail_unless(slabs[osc][fill] ==
					    pthread_self());
			}
			slab_unmap(&arena, slabs[osc]);
		}
	}
	slab_unmap(&arena, slabs);
	return 0;
}

void
bench(int count)
{
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	pthread_t *threads = (pthread_t *) malloc(sizeof(*threads)*count);

	int i;
	for (i = 0; i < count; i++) {
		pthread_create(&threads[i], &attr, run, NULL);
	}
	for (i = 0; i < count; i++) {
		pthread_t *thread = &threads[i];
		pthread_join(*thread, NULL);
	}
	free(threads);
}

int
main()
{
	plan(1);
	header();

	size_t maxalloc = THREADS * (OSCILLATION + 1) * SLAB_MIN_SIZE;
	quota_init(&quota, maxalloc);
	slab_arena_create(&arena, &quota, maxalloc/8,
			  SLAB_MIN_SIZE, MAP_PRIVATE);
	bench(THREADS);
	ok(true);
	slab_arena_destroy(&arena);

	footer();
	return check_plan();
}
