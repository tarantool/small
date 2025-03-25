#include <small/lf_lifo.h>
#include <sys/mman.h>
#include "unit.h"

#if !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

static void *
mmap_aligned(size_t size)
{
	assert((size & (size - 1)) == 0);
        void *map = mmap(NULL, 2 * size,
                         PROT_READ | PROT_WRITE, MAP_PRIVATE |
                         MAP_ANONYMOUS, -1, 0);

        /* Align the mapped address around slab size. */
        size_t offset = (intptr_t) map & (size - 1);

        if (offset != 0) {
                munmap(map, size - offset);
                map += size - offset;
                munmap(map + size, offset);
        } else {
                /* The address is returned aligned. */
                munmap(map + size, size);
        }
        return map;
}

#define MAP_SIZE 0x10000

static void
test_basic(void)
{
	plan(8);
	header();

	struct lf_lifo head;
	void *val1 = mmap_aligned(MAP_SIZE);
	void *val2 = mmap_aligned(MAP_SIZE);
	void *val3 = mmap_aligned(MAP_SIZE);
	lf_lifo_init(&head);

	ok(lf_lifo_pop(&head) == NULL);
	ok(lf_lifo_pop(lf_lifo_push(&head, val1)) == val1);
	ok(lf_lifo_pop(lf_lifo_push(&head, val1)) == val1);
	lf_lifo_push(lf_lifo_push(lf_lifo_push(&head, val1), val2), val3);
	ok(lf_lifo_pop(&head) == val3);
	ok(lf_lifo_pop(&head) == val2);
	ok(lf_lifo_pop(&head) == val1);
	ok(lf_lifo_pop(&head) == NULL);

	lf_lifo_init(&head);

	/* Test overflow of ABA counter. */

	int check = 1;
	do {
		lf_lifo_push(&head, val1);
		if (!(lf_lifo_pop(&head) == val1 && lf_lifo_pop(&head) == NULL))
			check = 0;
	} while (head.next != 0);
	ok(check);

	munmap(val1, MAP_SIZE);
	munmap(val2, MAP_SIZE);
	munmap(val3, MAP_SIZE);

	footer();
	check_plan();
}

static void
test_integer_pointer_conversion(void)
{
	plan(1);
	header();

	for (int i = 0; i < SMALL_LIFO_ALIGNMENT; i++)
		fail_unless((intptr_t)(void *)(uintptr_t)i == i);
	ok(true);

	footer();
	check_plan();
}

int
main(void)
{
	plan(2);
	header();

	test_integer_pointer_conversion();
	test_basic();

	footer();
	return check_plan();
}
