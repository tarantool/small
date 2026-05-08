#include <small/slab_arena.h>
#include <small/quota.h>
#include <small/util.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unit.h"

#ifndef ENABLE_ASAN

#define is_hex_digit(c)				\
	(((c) >= '0' && (c) <= '9')	||	\
	 ((c) >= 'a' && (c) <= 'f')	||	\
	 ((c) >= 'A' && (c) <= 'F'))

static bool
is_vma_range_fmt(char *line, unsigned long *start, unsigned long *end)
{
	char *p = line;
	while (*line && is_hex_digit(*line))
		line++;

	if (*line++ != '-')
		return false;

	while (*line && is_hex_digit(*line))
		line++;

	if (*line++ != ' ')
		return false;

	sscanf(p, "%lx-%lx", start, end);
	return true;
}

static bool
vma_has_flag(unsigned long vm_start, const char *flag)
{
	unsigned long start = 0, end = 0;
	char buf[1024], *tok = NULL;
	bool found = false;
	FILE *f = NULL;

	f = fopen("/proc/self/smaps", "r");
	fail_unless(f != NULL);

	while (fgets(buf, sizeof(buf), f)) {
		if (is_vma_range_fmt(buf, &start, &end))
			continue;
		if (strncmp(buf, "VmFlags: ", 9) || start != vm_start)
			continue;
		tok = buf;
		break;
	}

	if (tok) {
		for (tok = strtok(tok, " \n"); tok;
		     tok = strtok(NULL, " \n")) {
			if (strcmp(tok, flag))
				continue;
			found = true;
			break;
		}
	}

	fclose(f);
	return found;
}

static void
slab_test_madvise(void)
{
	plan(1);
	header();

	struct slab_arena arena;
	struct quota quota;
	void *ptr;

	/*
	 * The system doesn't support flags fetching.
	 */
	if (access("/proc/self/smaps", F_OK))
		goto finish;

	/*
	 * If system supports madvise call, test that
	 * preallocated area has been madvised.
	 */
	quota_init(&quota, 2000000);
	slab_arena_create(&arena, &quota, 3000000, 1,
			  SLAB_ARENA_PRIVATE | SLAB_ARENA_DONTDUMP);

	/*
	 * Will fetch from preallocated area.
	 */
	ptr = slab_map(&arena);
	fail_unless(ptr != NULL);
	fail_unless(vma_has_flag((unsigned long)ptr, "dd"));

	slab_unmap(&arena, ptr);
	slab_arena_destroy(&arena);

	/*
	 * A new slab for dynamic allocation.
	 */
	quota_init(&quota, 2000000);
	slab_arena_create(&arena, &quota, 0, 0x10000,
			  SLAB_ARENA_PRIVATE | SLAB_ARENA_DONTDUMP);

	/*
	 * Will fetch newly allocated area.
	 */
	ptr = slab_map(&arena);
	fail_unless(ptr != NULL);
	fail_unless(vma_has_flag((unsigned long)ptr, "dd"));

	slab_unmap(&arena, ptr);
	slab_arena_destroy(&arena);
finish:
	ok(true);

	footer();
	check_plan();
}

#else /* ifdef ENABLE_ASAN */

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
slab_test_membership(void)
{
	plan(1);
	header();

	struct slab_arena arena1;
	struct slab_arena arena2;

	slab_arena_create(&arena1, NULL, 0, 0, 0);
	slab_arena_create(&arena2, NULL, 0, 0, 0);

	void *ptr = slab_map(&arena1);
	fail_unless(ptr != NULL);
	small_on_assert_failure = on_assert_failure;
	slab_unmap(&arena2, ptr);
	small_on_assert_failure = small_on_assert_failure_default;
	ok(strstr(assert_msg_buf,
		  "object and arena id mismatch\" in slab_unmap") != NULL);

	footer();
	check_plan();
}

#endif /* ifdef ENABLE_ASAN */

static void
slab_test_basic(void)
{
	struct quota quota;
	struct slab_arena arena;

#ifdef ENABLE_ASAN
	plan(8);
#else
	plan(18);
#endif
	header();

	quota_init(&quota, 0);
	slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE);
	ok_no_asan(arena.prealloc == 0);
	ok(quota_total(&quota) == 0);
	ok_no_asan(arena.used == 0);
	ok(arena.slab_size == SLAB_MIN_SIZE);
	slab_arena_destroy(&arena);

#ifndef ENABLE_ASAN
	/* malloc implementation does not support quota. */
	quota_init(&quota, SLAB_MIN_SIZE);
	slab_arena_create(&arena, &quota, 1, 1, MAP_PRIVATE);
	ok(arena.prealloc == SLAB_MIN_SIZE);
	ok(quota_total(&quota) == SLAB_MIN_SIZE);
	ok(arena.used == 0);
	ok(arena.slab_size == SLAB_MIN_SIZE);
	void *ptr = slab_map(&arena);
	ok(ptr != NULL);
	ok(arena.used == SLAB_MIN_SIZE);
	void *ptr1 = slab_map(&arena);
	ok(ptr1 == NULL);
	ok(arena.used == SLAB_MIN_SIZE);
	slab_unmap(&arena, ptr);
	ok(arena.used == SLAB_MIN_SIZE);
	slab_unmap(&arena, ptr1);
	ok(arena.used == SLAB_MIN_SIZE);
	slab_arena_destroy(&arena);
#endif

	quota_init(&quota, 2000000);
	slab_arena_create(&arena, &quota, 3000000, 1, MAP_PRIVATE);
	ok_no_asan(arena.prealloc == 3014656);
	ok(quota_total(&quota) == 2000896);
	ok_no_asan(arena.used == 0);
	ok(arena.slab_size == SLAB_MIN_SIZE);
	slab_arena_destroy(&arena);

	footer();
	check_plan();
}

int
main(void)
{
	plan(2);
	header();

	slab_test_basic();
#ifdef ENABLE_ASAN
	slab_test_membership();
#else
	slab_test_madvise();
#endif

	footer();
	return check_plan();
}
