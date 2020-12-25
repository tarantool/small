#include <small/small.h>
#include <small/quota.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "../test/unit.h"

enum {
	OBJSIZE_MIN = 3 * sizeof(int),
	OBJECTS_MAX = 1000
};

const size_t SLAB_SIZE_MIN = 4 * 1024 * 1024;
const size_t SLAB_SIZE_MAX = 16 * 1024 * 1024;
static const unsigned long long NANOSEC_PER_SEC  = 1000000000;
static const unsigned long long NANOSEC_PER_MSEC  = 1000000;
#define SZR(arr) sizeof(arr) / sizeof(arr[0])

float slab_alloc_factor[] = {1.01, 1.03, 1.05, 1.1, 1.3, 1.5};

struct slab_arena arena;
struct slab_cache cache;
struct small_alloc alloc;
struct quota quota;
/** Streak type - allocating or freeing */
bool allocating = true;
/** Enable human output */
bool human = false;
/** Keep global to easily inspect the core. */
long seed;
char json_output[100000];
size_t length = sizeof(json_output);
size_t pos = 0;

static int *ptrs[OBJECTS_MAX];

static inline
long long int timediff(struct timespec *tm1, struct timespec *tm2)
{
	return NANOSEC_PER_SEC * (tm2->tv_sec - tm1->tv_sec) +
		(tm2->tv_nsec - tm1->tv_nsec);
}

static inline void
free_checked(int *ptr)
{
	int pos = ptr[0];
	smfree_delayed(&alloc, ptrs[pos], ptrs[pos][1]);
	ptrs[pos] = NULL;
}

static float
calculate_pow_factor(int size_max, int pow_max, int start)
{
	return exp(log((double)size_max / start) / pow_max);
}

static inline void *
alloc_checked(int pos, int size_min, int size_max, int rnd, double pow_factor)
{
	int size;
	if (ptrs[pos])
		free_checked(ptrs[pos]);

	if (!allocating)
		return NULL;

	if (rnd) {
		size = size_min + (rand() % (size_max - size_min));
	} else {
		size = floor(256 * pow(pow_factor, pos));
	}
	ptrs[pos] = smalloc(&alloc, size);
	if (ptrs[pos] == NULL)
		return NULL;
	ptrs[pos][0] = pos;
	ptrs[pos][1] = size;
	return ptrs[pos];
}

static int
small_is_unused_cb(const struct mempool_stats *stats, void *arg)
{
	unsigned long *slab_total = arg;
	*slab_total += stats->slabsize * stats->slabcount;
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
small_alloc_test(int size_min, int size_max, int iterations_max,
	int rnd, int cnt)
{
	double pow_factor = calculate_pow_factor(size_max, cnt, 256);
	for (int i = 0; i <= iterations_max; i++) {
		int mode = i % 3;
		switch (mode) {
		case 1:
			small_alloc_setopt(&alloc,
					   SMALL_DELAYED_FREE_MODE, false);
			break;
		case 2:
			small_alloc_setopt(&alloc,
					   SMALL_DELAYED_FREE_MODE, true);
			break;
		default:
			break;
		}
		for (int j = 0; j < cnt; ++j)
			alloc_checked(j, size_min, size_max, rnd, pow_factor);
		allocating = !allocating;
	}

	small_alloc_setopt(&alloc, SMALL_DELAYED_FREE_MODE, false);

	for (int pos = 0; pos < cnt; pos++) {
		if (ptrs[pos] != NULL)
			free_checked(ptrs[pos]);
	}

	/* Trigger garbage collection. */
	allocating = true;
	for (int i = 0; i < iterations_max; i++) {
		if (small_is_unused())
			break;
		void *p = alloc_checked(0, size_min, size_max, rnd, pow_factor);
		if (p != NULL)
			free_checked(p);
	}
}

static void
print_json_test_header(const char *type)
{
	size_t x = snprintf(json_output + pos, length,
			    "        \"%s\": {\n", type);
	length -= x;
	pos += x;
	x = snprintf(json_output + pos, length,
		     "            \"alloc factor\": {\n");
	length -= x;
	pos += x;
	for (unsigned int i = 0; i < SZR(slab_alloc_factor); i++) {
		size_t x = snprintf(json_output + pos, length,
				    "                \"%.4f\"\n",
				    slab_alloc_factor[i]);
		length -= x;
		pos += x;
	}
	x = snprintf(json_output + pos, length, "            },\n");
	length -= x;
	pos += x;
	x = snprintf(json_output + pos, length,
		     "            \"time, s\": {\n");
	length -= x;
	pos += x;
}

static void
print_json_test_finish(const char * finish)
{
	size_t x = snprintf(json_output + pos, length, "            }\n");
	length -= x;
	pos += x;
	x = snprintf(json_output + pos, length, "        }%s\n", finish);
	length -= x;
	pos += x;
}

static void
print_json_test_result(double time)
{
	size_t x = snprintf(json_output + pos, length,
			    "                \"%.3f\"\n", time);
	length -= x;
	pos += x;
}

static void
small_alloc_basic(unsigned int slab_size)
{
	struct timespec tm1, tm2;
	if(human) {
		fprintf(stderr, "|              SMALL RANDOM "
			"ALLOCATION RESULT TABLE                  |\n");
		fprintf(stderr, "|___________________________________"
			"_________________________________|\n");
		fprintf(stderr, "|           alloc_factor          "
			" |   	         time, ms            |\n");
		fprintf(stderr, "|__________________________________|"
			"_________________________________|\n");
	} else {
		print_json_test_header("random");
	}
	quota_init(&quota, UINT_MAX);
	slab_arena_create(&arena, &quota, 0, slab_size, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);
	for (unsigned int i = 0; i < SZR(slab_alloc_factor); i++) {
		float actual_alloc_factor;
		small_alloc_create(&alloc, &cache,
				   OBJSIZE_MIN, slab_alloc_factor[i],
				   &actual_alloc_factor);
		int size_min = OBJSIZE_MIN;
		int size_max = (int)alloc.objsize_max - 1;
		fail_unless(clock_gettime (CLOCK_MONOTONIC, &tm1) == 0);
		small_alloc_test(size_min, size_max, 300, 1, OBJECTS_MAX);
		fail_unless(clock_gettime (CLOCK_MONOTONIC, &tm2) == 0);
		if (human) {
			fprintf(stderr, "|              %.4f              |"
				"             %6llu              |\n",
				slab_alloc_factor[i],
				timediff(&tm1, &tm2) / NANOSEC_PER_MSEC);
		} else {
			print_json_test_result(timediff(&tm1, &tm2) /
					       NANOSEC_PER_MSEC);
		}
		small_alloc_destroy(&alloc);
	}
	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);
	quota_init(&quota, UINT_MAX);
	slab_arena_create(&arena, &quota, 0, slab_size, MAP_PRIVATE);
	slab_cache_create(&cache, &arena);
	if (human) {
		fprintf(stderr, "|__________________________________|"
			"_________________________________|\n");
		fprintf(stderr, "|             SMALL EXP GROW "
			"ALLOCATION RESULT TABLE                 |\n");
		fprintf(stderr, "|___________________________________"
			"_________________________________|\n");
		fprintf(stderr, "|           alloc_factor          "
			" |   	         time, ms            |\n");
		fprintf(stderr, "|__________________________________|"
			"_________________________________|\n");
	} else {
		print_json_test_finish(",");
		print_json_test_header("exponent");
	}
	for (unsigned int i = 0; i < SZR(slab_alloc_factor); i++) {
		float actual_alloc_factor;
		small_alloc_create(&alloc, &cache,
				   OBJSIZE_MIN, slab_alloc_factor[i],
				   &actual_alloc_factor);
		int size_min = OBJSIZE_MIN;
		int size_max = (int)alloc.objsize_max - 1;
		fail_unless(clock_gettime (CLOCK_MONOTONIC, &tm1) == 0);
		small_alloc_test(size_min, size_max, 1000, 0, OBJECTS_MAX);
		fail_unless(clock_gettime (CLOCK_MONOTONIC, &tm2) == 0);
		if (human) {
			fprintf(stderr, "|              %.4f              |"
				"             %6llu              |\n",
				slab_alloc_factor[i],
				timediff(&tm1, &tm2) / NANOSEC_PER_MSEC);
		} else {
			print_json_test_result(timediff(&tm1, &tm2) /
					       NANOSEC_PER_MSEC);
		}
		small_alloc_destroy(&alloc);
	}
	if (human) {
		fprintf(stderr, "|___________________________________"
			"_________________________________|\n");
	} else {
		print_json_test_finish(",");
	}
	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);
}

static void
small_alloc_large()
{
	struct timespec tm1, tm2;
	size_t large_size_min = mempool_objsize_max(cache.arena->slab_size);
	size_t large_size_max = 2 * cache.arena->slab_size;
	if (human) {
		fprintf(stderr, "|              LARGE RANDOM "
			"ALLOCATION RESULT TABLE                  |\n");
		fprintf(stderr, "|___________________________________"
			"_________________________________|\n");
		fprintf(stderr, "|           alloc_factor          "
			" |   	         time, ms            |\n");
		fprintf(stderr, "|__________________________________|"
			"_________________________________|\n");
	} else {
		print_json_test_header("large");
	}
	for (unsigned int i = 0; i < SZR(slab_alloc_factor); i++) {
		float actual_alloc_factor;
		small_alloc_create(&alloc, &cache, OBJSIZE_MIN,
				   slab_alloc_factor[i], &actual_alloc_factor);
		fail_unless(clock_gettime (CLOCK_MONOTONIC, &tm1) == 0);
		small_alloc_test(large_size_min, large_size_max, 200, 1, 25);
		fail_unless(clock_gettime (CLOCK_MONOTONIC, &tm2) == 0);
		if (human) {
			fprintf(stderr, "|              %.4f              |"
				"             %6llu              |\n",
				slab_alloc_factor[i],
				timediff(&tm1, &tm2) / NANOSEC_PER_MSEC);
		} else {
			print_json_test_result(timediff(&tm1, &tm2) /
					       NANOSEC_PER_MSEC);
		}
		small_alloc_destroy(&alloc);
	}
	if (human) {
		fprintf(stderr, "|___________________________________"
			"_________________________________|\n");
	} else {
		print_json_test_finish("");
	}
}

int main(int argc, char* argv[])
{
	size_t x;
	seed = time(0);
	srand(seed);

	if (argc == 2 && !strcmp(argv[1], "-h")) //human clear output
		human = true;

	if (!human) {
		x = snprintf(json_output + pos, length, "{\n");
		length -= x;
		pos += x;
	}
	for (unsigned int slab_size = SLAB_SIZE_MIN; slab_size <= SLAB_SIZE_MAX;
	     slab_size *= 2) {
		if(human) {
			fprintf(stderr, "_____________________________________"
				"_________________________________\n");
			fprintf(stderr, "|           PERFORMANCE TEST WITH SLABSIZE "
				"%8u BYTES            |\n", slab_size);
			fprintf(stderr, "|___________________________________"
				"_________________________________|\n");
		} else {
			size_t x = snprintf(json_output + pos, length,
					    "    \"test\": {\n");
			length -= x;
			pos += x;
			x = snprintf(json_output + pos, length,
				     "        \"slab size, bytes\": \"%u\",\n",
				     slab_size);
			length -= x;
			pos += x;
		}
		small_alloc_basic(slab_size);
		quota_init(&quota, UINT_MAX);
		slab_arena_create(&arena, &quota, 0, slab_size, MAP_PRIVATE);
		slab_cache_create(&cache, &arena);
		small_alloc_large();
		slab_cache_destroy(&cache);
		slab_arena_destroy(&arena);
		if (!human) {
			x = snprintf(json_output + pos, length,
				     "    }%s\n",
				     (slab_size == SLAB_SIZE_MAX ? "" : ","));
			length -= x;
			pos += x;
		}
	}
	if (!human) {
		x = snprintf (json_output + pos, length, "}\n");
		fprintf(stderr, "%s\n", json_output);
	}
	return EXIT_SUCCESS;
}
