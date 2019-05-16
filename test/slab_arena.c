#include <small/slab_arena.h>
#include <small/quota.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "unit.h"

void
slab_arena_print(struct slab_arena *arena)
{
	printf("arena->prealloc = %zu\narena->maxalloc = %zu\n"
	       "arena->used = %zu\narena->slab_size = %u\n",
	       arena->prealloc, quota_total(arena->quota),
	       arena->used, arena->slab_size);
}

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
	if (!f) {
		printf("ERROR: Can't open smaps for %lx\n", vm_start);
		return false;
	}

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
	struct slab_arena arena;
	struct quota quota;
	void *ptr;

	/*
	 * The system doesn't support flags fetching.
	 */
	if (access("/proc/self/smaps", F_OK))
		return;

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
	if (!ptr) {
		printf("ERROR: can't obtain preallocated slab\n");
		goto out;
	}

	if (!vma_has_flag((unsigned long)ptr, "dd"))
		goto no_dd;

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
	if (!ptr) {
		printf("ERROR: can't obtain dynamic slab\n");
		goto out;
	}

	if (!vma_has_flag((unsigned long)ptr, "dd"))
		goto no_dd;

out:
	slab_unmap(&arena, ptr);
	slab_arena_destroy(&arena);
	return;
no_dd:
	printf("ERROR: Expected dd flag on VMA address %p\n", ptr);
	goto out;
}

int main()
{
	struct quota quota;
	struct slab_arena arena;

	quota_init(&quota, 0);
	slab_arena_create(&arena, &quota, 0, 0, MAP_PRIVATE);
	slab_arena_print(&arena);
	slab_arena_destroy(&arena);

	quota_init(&quota, SLAB_MIN_SIZE);
	slab_arena_create(&arena, &quota, 1, 1, MAP_PRIVATE);
	slab_arena_print(&arena);
	void *ptr = slab_map(&arena);
	slab_arena_print(&arena);
	void *ptr1 = slab_map(&arena);
	printf("going beyond the limit: %s\n", ptr1 ? "(ptr)" : "(nil)");
	slab_arena_print(&arena);
	slab_unmap(&arena, ptr);
	slab_unmap(&arena, ptr1);
	slab_arena_print(&arena);
	slab_arena_destroy(&arena);

	quota_init(&quota, 2000000);
	slab_arena_create(&arena, &quota, 3000000, 1, MAP_PRIVATE);
	slab_arena_print(&arena);
	slab_arena_destroy(&arena);

	slab_test_madvise();
}
