#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include "util.h"
#include "unit.h"

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
test_asan_poison_precise(const char *buf, int size, int start, int end)
{
	for (int i = 0; i < size; i++) {
		int p = __asan_address_is_poisoned(buf + i);
		if (i < start || i >= end) {
			fail_if(p);
		} else {
			fail_unless(p);
		}
	}
}

/**
 * Check assumptions about ASAN poison/unpoison alignment. Poison/unpoison
 * is precise if range end is 8-aligned or range end is end of memory allocated
 * with malloc().
 */
static void
test_asan_poison_assumptions(void)
{
	plan(1);
	header();

	/* Test poison when range begin is arbitrary and end is 8-aligned. */
	int size = SMALL_POISON_ALIGNMENT * 17;
	char *buf = malloc(size);
	for (int n = 0; n < 100; n++) {
		int start = rand() % 17 * SMALL_POISON_ALIGNMENT;
		int end = start + (1 + rand() % (17 - start));
		end *= SMALL_POISON_ALIGNMENT;
		for (int i = 0; i < SMALL_POISON_ALIGNMENT; i++) {
			ASAN_POISON_MEMORY_REGION(buf + start + i,
						  end - start - i);
			test_asan_poison_precise(buf, size, start + i, end);
			ASAN_UNPOISON_MEMORY_REGION(buf + start + i,
						    end - start - i);
		}
	}
	free(buf);

	/* Test poison range end is end of memory allocated with malloc. */
	for (int n = 0; n < 1000; n++) {
		int size = 1 + rand() % 333;
		buf = malloc(size);
		int start = rand() % size;
		ASAN_POISON_MEMORY_REGION(buf + start, size - start);
		test_asan_poison_precise(buf, size, start, size);
		ASAN_UNPOISON_MEMORY_REGION(buf + start, size - start);
		free(buf);
	}
	ok(true);

	footer();
	check_plan();
}

static void
test_asan_alloc_run(size_t payload_size, size_t alignment, size_t header_size)
{
	char *header = small_asan_alloc(payload_size, alignment, header_size);
	fail_unless(header != NULL);
	char *payload = small_asan_payload_from_header(header);
	fail_unless(payload != NULL);
	memset(payload, 0, payload_size);
	fail_unless(payload >= header + header_size);
	fail_unless((uintptr_t)payload % alignment == 0);
	fail_unless((uintptr_t)payload % (2 * alignment) != 0);
	fail_unless(small_asan_header_from_payload(payload) == header);

	char *magic_begin = (char *)small_align_down((uintptr_t)payload,
						     SMALL_POISON_ALIGNMENT);
	fail_unless(magic_begin >= header + header_size);
	fail_unless(__asan_address_is_poisoned(header - 1));
	fail_unless(__asan_address_is_poisoned(payload + payload_size));
	/* Check poison up until magic. */
	for (char *p = header; p < magic_begin; p++)
		fail_unless(__asan_address_is_poisoned(p));
	small_asan_free(header);
	/* Check magic works. */
	size_t magic_size = payload - magic_begin;
	for (size_t off = 0; off < magic_size; off++) {
		header = small_asan_alloc(payload_size, alignment, header_size);
		payload = small_asan_payload_from_header(header);
		magic_begin = (char *)small_align_down((uintptr_t)payload,
						       SMALL_POISON_ALIGNMENT);
		char *p = magic_begin + off;
		fail_unless(*p != 0);
		*p = '\0';
		small_on_assert_failure = on_assert_failure;
		assert_msg_buf[0] = '\0';
		small_asan_free(header);
		small_on_assert_failure = small_on_assert_failure_default;
		p = strstr(assert_msg_buf,
			   "corrupted magic\" in small_asan_free");
		fail_unless(p != NULL);
	}
}

static void
test_asan_alloc(void)
{
	plan(1);
	header();

	size_t header_sizes[] = {0, 1, 2, 3, 4, 8, 12, 16};
	for (int i = 0; i < (int)lengthof(header_sizes); i++) {
		for (int j = 0; j < 5; j++) {
			size_t alignment = 1 << j;
			for (int k = 0; k < 11; k++) {
				size_t payload_size = alignment * k;
				test_asan_alloc_run(payload_size, alignment,
						    header_sizes[i]);
			}
		}
	}
	ok(true);

	footer();
	check_plan();
}

#endif /* ifdef ENABLE_ASAN */

static void
test_align_down(void)
{
	plan(1);
	header();

	for (int i = 0; i < 6; i++) {
		size_t alignment = 1 << i;
		for (size_t size = 0; size < 117; size++) {
			size_t r = small_align_down(size, alignment);
			fail_unless(r % alignment == 0);
			fail_unless(r <= size);
			fail_unless(size - r < alignment);
		}
	}
	ok(true);

	footer();
	check_plan();
}

int
main(void)
{
#ifdef ENABLE_ASAN
	plan(3);
#else
	plan(1);
#endif

	unsigned int seed = time(NULL);
	note("random seed is %u", seed);
	srand(seed);

#ifdef ENABLE_ASAN
	test_asan_poison_assumptions();
	test_asan_alloc();
#endif
	test_align_down();

	return check_plan();
}
