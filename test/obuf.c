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
#ifdef ENABLE_ASAN
	OSCILLATION_MAX = SMALL_OBUF_IOV_CHECKED_MAX * 3,
#else
	OSCILLATION_MAX = 1024,
#endif
	ITERATIONS_MAX = 1000,
};

void
alloc_checked(struct obuf *buf)
{
	int size = OBJSIZE_MIN + rand() % (OBJSIZE_MAX - OBJSIZE_MIN + 1);
	fail_unless(size >= OBJSIZE_MIN && size <= OBJSIZE_MAX);
	void *ptr = obuf_alloc(buf, size);
	fail_unless(ptr != NULL);
#ifdef ENABLE_ASAN
	fail_unless(buf->iov[buf->pos + 1].iov_base == NULL);
	fail_unless(buf->iov[buf->pos + 1].iov_len == 0);
	if (buf->pos < SMALL_OBUF_IOV_CHECKED_MAX) {
		fail_unless((uintptr_t)ptr % SMALL_OBUF_ALIGNMENT == 0);
		fail_unless((uintptr_t)ptr % (2 * SMALL_OBUF_ALIGNMENT) != 0);
	}
#endif
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

static void
obuf_rollback_run(struct slab_cache *slabc)
{
	struct obuf buf;
	obuf_create(&buf, slabc, 16384);
	struct obuf_svp *svp = malloc(sizeof(*svp) * OSCILLATION_MAX);
	struct iovec *iov = malloc(sizeof(*iov) * OSCILLATION_MAX);

	int i;
	for (i = 0; i < OSCILLATION_MAX; i++) {
		iov[i] = buf.iov[buf.pos];
		svp[i] = obuf_create_svp(&buf);
		int size = OBJSIZE_MIN +
			   rand() % (OBJSIZE_MAX - OBJSIZE_MIN + 1);
		void *ptr = obuf_alloc(&buf, size);
		fail_unless(ptr != NULL);
	}
	i -= 1 + rand() % 6;
	while (i > 0) {
		obuf_rollback_to_svp(&buf, &svp[i]);
		fail_unless(buf.pos == (int)svp[i].pos);
		fail_unless(buf.iov[buf.pos].iov_len == svp[i].iov_len);
		fail_unless(buf.used == svp[i].used);
		fail_unless(buf.iov[buf.pos].iov_base == iov[i].iov_base);
		((char *)iov[i].iov_base)[0] = '\0';
		((char *)iov[i].iov_base)[iov[i].iov_len - 1] = '\0';
		for (int pos = buf.pos + 1; pos <= SMALL_OBUF_IOV_MAX; pos++) {
			fail_unless_asan(buf.iov[pos].iov_base == NULL);
			fail_unless_asan(buf.iov[pos].iov_len == 0);
		}
		i -= rand() % 7;
	}
	obuf_rollback_to_svp(&buf, &svp[0]);
	fail_unless(buf.pos == 0);
	fail_unless(buf.used == 0);
	for (int pos = 0; pos <= SMALL_OBUF_IOV_MAX; pos++) {
		fail_unless_asan(buf.iov[pos].iov_base == NULL);
		fail_unless_asan(buf.iov[pos].iov_len == 0);
	}

	obuf_destroy(&buf);
	free(svp);
	free(iov);
}

static void
obuf_rollback(struct slab_cache *slabc)
{
	plan(1);
	header();

	for (int i = 0; i < 37; i++)
		obuf_rollback_run(slabc);
	ok(true);

	footer();
	check_plan();
}

#ifdef ENABLE_ASAN

static void
obuf_poison(struct slab_cache *slabc)
{
	plan(1);
	header();

	struct obuf buf;
	obuf_create(&buf, slabc, 16384);
	size_t size_max = 2 * small_getpagesize();
	for (int i = 0; i < 7777; i++) {
		size_t size_r = 1 + rand() % size_max;
		size_t size_a = 1 + rand() % size_r;
		char *ptr_r = obuf_reserve(&buf, size_r);
		fail_unless(ptr_r != NULL);
		size_t reserved = buf.capacity[buf.pos] -
				  buf.iov[buf.pos].iov_len;
		ptr_r[0] = 0;
		ptr_r[reserved - 1] = 0;
		fail_unless(__asan_address_is_poisoned(ptr_r + reserved));
		char *ptr_a = obuf_alloc(&buf, size_a);
		fail_unless(ptr_r == ptr_a);
		fail_unless(__asan_address_is_poisoned(ptr_a + size_a));
	}
	obuf_destroy(&buf);
	ok(true);

	footer();
	check_plan();
}

static void
obuf_tiny_reserve_size(struct slab_cache *slabc)
{
	plan(1);
	header();

	struct obuf buf;
	obuf_create(&buf, slabc, 16384);
	size_t size_max = SMALL_OBUF_MIN_RESERVE;
	for (int i = 0; i < SMALL_OBUF_IOV_CHECKED_MAX; i++) {
		size_t size = 1 + rand() % size_max;
		void *ptr = obuf_reserve(&buf, size);
		fail_unless(ptr != NULL);
		memset(ptr, 0, size_max);
		obuf_alloc(&buf, size);
	}
	obuf_destroy(&buf);
	ok(true);

	footer();
	check_plan();
}

#endif /* ifdef ENABLE_ASAN */

int main()
{
	struct slab_cache cache;
	struct slab_arena arena;
	struct quota quota;

#ifdef ENABLE_ASAN
	plan(4);
#else
	plan(2);
#endif
	header();

	unsigned int seed = time(NULL);
	note("random seed is %u", seed);
	srand(seed);

	quota_init(&quota, UINT_MAX);

	slab_arena_create(&arena, &quota, 0, 4000000,
			  MAP_PRIVATE);
	slab_cache_create(&cache, &arena);

	obuf_basic(&cache);
	obuf_rollback(&cache);
#ifdef ENABLE_ASAN
	obuf_poison(&cache);
	obuf_tiny_reserve_size(&cache);
#endif

	slab_cache_destroy(&cache);
	slab_arena_destroy(&arena);

	footer();
	return check_plan();
}
