#include <iostream>
#include "unit.h"
#include "../small/rb.h"
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <stdint.h>
#include "assert.h"
#include <string.h>
#include <set>
#include <utility>
#include <algorithm>

#define MAX_KEY 30
#define RB_COMPACT 0
#define DEFAULT_NODES 100
#define NUMBER_OPERS 5000

typedef struct node_s node_t;

typedef std::pair<int, int> my_pair;

struct node_s {
	rb_node(node_t) node;
	my_pair key;
};

typedef rb_tree(node_t) tree_t;
typedef std::set<my_pair> my_set;

static inline int
key_cmp(const my_pair &a, const my_pair &b)
{
	return (a.first > b.first) ? 1 : (a.first < b.first) ? -1 :
		(a.second > b.second) ? 1 : (a.second < b.second) ? -1: 0;
}

static inline int
key_node_cmp(const my_pair &a , const node_t *b)
{
	return key_cmp(a, b->key);
}

static inline int
node_cmp(const node_t *a, const node_t *b)
{
	return key_cmp(a->key, b->key);
}

rb_gen_ext_key(static inline MAYBE_UNUSED, test_, tree_t,
		   node_t, node, node_cmp,
		   my_pair, key_node_cmp);

enum OPERS {
	INSERT = 0,
	DELETE,
	SEARCH,
	SEARCH_GE,
	SEARCH_LE,
	SEARCH_GT,
	SEARCH_LT,
};

void
insert(tree_t *tree, my_set& stl_tree)
{
	node_t *node = (node_t *) calloc(1, sizeof(*node));
	if (!node)
		return;
	node->key = std::make_pair(rand() % MAX_KEY, rand() % MAX_KEY);
	auto res = stl_tree.insert(node->key);
	if (res.second) {
		test_insert(tree, node);
	}
}


void
remove(tree_t *tree, my_set &stl_tree) {
	my_pair key = std::make_pair(rand() % MAX_KEY, rand() % MAX_KEY);
	node_t * res = test_search(tree, key);
	if (res) {
		/* without check segfault */
		stl_tree.erase(key);
		test_remove(tree, res);
		free(res);
	}
}

void
filling(tree_t *tree, my_set& stl_tree)
{
	for (int i = 0; i < DEFAULT_NODES; i++) {
		insert(tree, stl_tree);
	}
}

#define check(stl_it, stl_tree, res, res_it)				\
if (stl_it == stl_tree.end()) {						\
		fail_unless(res == NULL);				\
		fail_unless(res_it == NULL);				\
} else {								\
		fail_unless(res != NULL &&				\
				key_node_cmp((*stl_it), res) == 0);	\
		fail_unless(res_it != NULL &&				\
				key_node_cmp((*stl_it), res_it) == 0);	\
	}

void
search(tree_t *tree, my_set& stl_tree)
{
	my_pair key = std::make_pair(rand() % MAX_KEY, rand() % MAX_KEY);
	node_t *res = test_search(tree, key);
	test_iterator it;
	test_isearch(tree, key, &it);
	node_t *res_it = test_iterator_get(&it);
	auto stl_it = stl_tree.find(key);
	check(stl_it, stl_tree, res, res_it);
}

void
search_ge(tree_t *tree, my_set& stl_tree)
{
	my_pair key = std::make_pair(rand() % MAX_KEY, rand() % MAX_KEY);
	node_t *res = test_nsearch(tree, key);
	test_iterator it;
	test_isearch_ge(tree, key, &it);
	node_t *res_it = test_iterator_get(&it);
	auto stl_it = stl_tree.lower_bound(key);
	/* lower bound is first not less, a.k.a greater or equal */
	check(stl_it, stl_tree, res, res_it);
}

void
search_gt(tree_t *tree, my_set& stl_tree)
{
	my_pair key = std::make_pair(rand() % MAX_KEY, rand() % MAX_KEY);
	test_iterator it;
	test_isearch_gt(tree, key, &it);
	node_t *res_it = test_iterator_get(&it);
	auto stl_it = stl_tree.upper_bound(key);
	check(stl_it, stl_tree, res_it, res_it);
}

void
search_le(tree_t *tree, my_set& stl_tree)
{
	my_pair key = std::make_pair(rand() % MAX_KEY, rand() % MAX_KEY);
	node_t *res = test_psearch(tree, key);
	test_iterator it;
	test_isearch_le(tree, key, &it);
	node_t *res_it = test_iterator_get(&it);
	my_set::iterator stl_it = stl_tree.upper_bound(key);
	/* upper_bound is one step further than le */
	if (stl_it == stl_tree.begin()) {
		/* begin is not decrementable */
		stl_it = stl_tree.end();
	} else {
		stl_it--;
	}
	check(stl_it, stl_tree, res, res_it);
}

void
search_lt(tree_t *tree, my_set& stl_tree)
{
	my_pair key = std::make_pair(rand() % MAX_KEY, rand() % MAX_KEY);
	test_iterator it;
	test_isearch_lt(tree, key, &it);
	node_t *res_it = test_iterator_get(&it);
	my_set::iterator stl_it = stl_tree.lower_bound(key);
	/* lower_bound is one step further than lt */
	if (stl_it == stl_tree.begin()) {
		/* begin is not decrementable */
		stl_it = stl_tree.end();
	} else if (stl_it == stl_tree.end()) {
		/* if key is bigger than the largest in tree */
		stl_it = std::max_element(stl_tree.begin(), stl_tree.end());
	} else {
		stl_it--;
	}
	check(stl_it, stl_tree, res_it, res_it);
}

void
opers(tree_t *tree, my_set& stl_tree)
{
	for (int i = 0; i < NUMBER_OPERS; i++) {
		int op = rand() % 7;
		switch (op) {
			case INSERT:
				insert(tree, stl_tree);
				break;
			case DELETE:
				remove(tree, stl_tree);
				break;
			case SEARCH:
				search(tree, stl_tree);
				break;
			case SEARCH_GE:
				search_ge(tree, stl_tree);
				break;
			case SEARCH_LE:
				search_le(tree, stl_tree);
				break;
			case SEARCH_GT:
				search_gt(tree, stl_tree);
				break;
			case SEARCH_LT:
				search_lt(tree, stl_tree);
				break;
		}

	}
}

void delete_all(tree_t *tree, my_set& stl_tree)
{
	for (auto it = stl_tree.begin(); it != stl_tree.end(); ++it){
		my_pair p = *it;
		node_t *n = test_search(tree, p);
		fail_unless(n);
		test_remove(tree, n);
		free(n);
	}
	stl_tree.clear();
}

int main()
{
	plan(1);
	header();

	unsigned int seed = time(NULL);
	note("random seed is %u", seed);
	srand(seed);
	tree_t tree;
	test_new(&tree);
	my_set stl_tree;
	filling(&tree, stl_tree);
	opers(&tree, stl_tree);
	/* clear all remaining in tree*/
	delete_all(&tree, stl_tree);
	ok(true);

	footer();
	check_plan();
}
