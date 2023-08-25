#include "unit.h"
#include "../small/rlist.h"

#define ITEMS		7

struct test {
	int no;
	struct rlist list;
};

static struct test items[ITEMS];

static RLIST_HEAD(head);
static RLIST_HEAD(head2);

int
main(void)
{
	int i;
	struct test *it, *tmp;
	struct rlist *rlist;

	plan(104);
	header();

	ok(rlist_empty(&head), "list is empty");
	for (i = 0; i < ITEMS; i++) {
		items[i].no = i;
		rlist_add_tail(&head, &(items[i].list));
	}
	RLIST_HEAD(empty_list);
	ok(rlist_empty(&empty_list), "rlist_nil is empty");
	ok(rlist_empty(&head2), "head2 is empty");
	rlist_swap(&head2, &empty_list);
	ok(rlist_empty(&empty_list), "rlist_nil is empty after swap");
	ok(rlist_empty(&head2), "head2 is empty after swap");
	rlist_swap(&head, &head2);
	ok(rlist_empty(&head), "head is empty after swap");
	is(rlist_first(&head2), &items[0].list, "first item");
	is(rlist_last(&head2), &items[ITEMS - 1].list, "last item");
	i = 0;
	rlist_foreach(rlist, &head2) {
		is(rlist, &items[i].list, "element (foreach) %d", i);
		i++;
	}
	rlist_foreach_reverse(rlist, &head2) {
		i--;
		is(rlist, &items[i].list, "element (foreach_reverse) %d", i);
	}
	rlist_swap(&head2, &head);


	is(rlist_first(&head), &items[0].list, "first item");
	isnt(rlist_first(&head), &items[ITEMS - 1].list, "first item");

	is(rlist_last(&head), &items[ITEMS - 1].list, "last item");
	isnt(rlist_last(&head), &items[0].list, "last item");

	is(rlist_next(&head), &items[0].list, "rlist_next");
	is(rlist_prev(&head), &items[ITEMS - 1].list, "rlist_prev");

	i = 0;
	rlist_foreach(rlist, &head) {
		is(rlist, &items[i].list, "element (foreach) %d", i);
		i++;
	}
	rlist_foreach_reverse(rlist, &head) {
		i--;
		is(rlist, &items[i].list, "element (foreach_reverse) %d", i);
	}


	is(rlist_entry(&items[0].list, struct test, list), &items[0],
		"rlist_entry");
	is(rlist_first_entry(&head, struct test, list), &items[0],
		"rlist_first_entry");
	is(rlist_next_entry(&items[0], list), &items[1], "rlist_next_entry");
	is(rlist_prev_entry(&items[2], list), &items[1], "rlist_prev_entry");


	i = 0;
	rlist_foreach_entry(it, &head, list) {
		is(it, items + i, "element (foreach_entry) %d", i);
		i++;
	}
	rlist_foreach_entry_reverse(it, &head, list) {
		i--;
		is(it, items + i, "element (foreach_entry_reverse) %d", i);
	}

	rlist_del(&items[2].list);
	ok(rlist_empty(&head2), "head2 is empty");
	rlist_move(&head2, &items[3].list);
	ok(!rlist_empty(&head2), "head2 isnt empty");
	is(rlist_first_entry(&head2, struct test, list),
					&items[3], "Item was moved");
	rlist_move_tail(&head2, &items[4].list);
	rlist_foreach_entry(it, &head, list) {
		is(it, items + i, "element (second deleted) %d", i);
		i++;
		if (i == 2)
			i += 3;
	}
	rlist_foreach_entry_reverse(it, &head, list) {
		i--;
		if (i == 4)
			i -= 3;
		is(it, items + i, "element (second deleted) %d", i);
	}


	rlist_create(&head);
	ok(rlist_empty(&head), "list is empty");
	for (i = 0; i < ITEMS; i++) {
		items[i].no = i;
		rlist_add(&head, &(items[i].list));
	}
	i = 0;
	rlist_foreach_entry_reverse(it, &head, list) {
		is(it, items + i, "element (foreach_entry_reverse) %d", i);
		i++;
	}
	rlist_foreach_entry(it, &head, list) {
		i--;
		is(it, items + i, "element (foreach_entry) %d", i);
	}
	rlist_create(&head);
	rlist_add_entry(&head, &items[0], list);
	ok(rlist_prev_entry_safe(&items[0], &head, list) == NULL,
	   "prev is null");

	rlist_create(&head);
	i = 0;
	rlist_foreach_entry_safe_reverse(it, &head, list, tmp)
		++i;
	ok(i == 0, "list is empty");
	for (i = 0; i < ITEMS; i++) {
		items[i].no = i;
		rlist_add(&head, &(items[i].list));
	}
	i = 0;
	rlist_foreach_entry_safe_reverse(it, &head, list, tmp) {
		rlist_del_entry(it, list);
		is(it, items + i, "element (reverse) %d", i);
		i++;
	}
	rlist_add(&head, &items[0].list);
	rlist_cut_before(&head2, &head, head.next);
	ok(rlist_empty(&head2), "list is empty");
	for (i = 1; i < ITEMS; i++) {
		items[i].no = i;
		rlist_add_tail(&head, &(items[i].list));
	}
	rlist_cut_before(&head2, &head, head.next);
	ok(rlist_empty(&head2), "list is empty");
	rlist_cut_before(&head2, &head, &items[ITEMS / 2].list);
	i = 0;
	rlist_foreach_entry(it, &head2, list) {
		is(it, items + i, "element (first half) %d", i);
		i++;
	}
	rlist_foreach_entry(it, &head, list) {
		is(it, items + i, "element (second half) %d", i);
		i++;
	}

	footer();
	return check_plan();
}
