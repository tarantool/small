#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "unit.h"

#define RB_COMPACT 1
#include "../small/rb.h"

/*
 * Weighted binary search tree.
 *
 * This is an augmented version of a standard binary search tree
 * where each node maintains the size of the sub-tree rooted at it.
 * Allows to efficiently (in log N) calculate the rank of each node.
 */

struct wtree_node {
	/* Link in a binary search tree. */
	rb_node(struct wtree_node) in_tree;
	/* Value associated with this node. */
	int value;
	/* Size of the sub-tree rooted at this node. */
	int weight;
};

typedef struct wtree_node wtree_node_t;
typedef rb_tree(struct wtree_node) wtree_t;

rb_proto_ext_key(static, wtree_, wtree_t, wtree_node_t, int);

static int
wtree_node_cmp(const wtree_node_t *a, const wtree_node_t *b)
{
	int rc = a->value < b->value ? -1 : a->value > b->value;
	if (rc == 0)
		rc = a < b ? -1 : a > b;
	return rc;
}

static int
wtree_key_node_cmp(const int key, const wtree_node_t *node)
{
	return key < node->value ? -1 : key > node->value;
}

static void
wtree_node_aug(wtree_node_t *node,
	       const wtree_node_t *left,
	       const wtree_node_t *right)
{
	node->weight = 1;
	if (left != NULL)
		node->weight += left->weight;
	if (right != NULL)
		node->weight += right->weight;
}

rb_gen_ext_key_aug(MAYBE_UNUSED static, wtree_, wtree_t, wtree_node_t, in_tree,
		   wtree_node_cmp, int, wtree_key_node_cmp, wtree_node_aug);

static void
wtree_selfcheck(wtree_t *tree)
{
	wtree_node_t *node, *prev, *left, *right;

	/* Check node order. */
	struct wtree_iterator it;
	wtree_ifirst(tree, &it);
	prev = NULL;
	while ((node = wtree_inext(&it)) != NULL) {
		if (prev != NULL)
			fail_unless(prev->value <= node->value);
		prev = node;
	}

	/* Check node weights. */
	struct wtree_walk walk;
	wtree_walk_init(&walk, tree);
	while ((node = wtree_walk_next(&walk, RB_WALK_RIGHT | RB_WALK_LEFT,
				       &left, &right)) != NULL) {
		int left_weight = left != NULL ? left->weight : 0;
		int right_weight = right != NULL ? right->weight : 0;
		fail_unless(node->weight == left_weight + right_weight + 1);
	}
}

/*
 * Return the number of elements in the tree that are
 * less than the given value.
 */
static int
wtree_rank(wtree_t *tree, int value)
{
	int dir = 0;
	int count = 0;
	struct wtree_walk walk;
	wtree_walk_init(&walk, tree);
	wtree_node_t *node, *left, *right;
	while ((node = wtree_walk_next(&walk, dir, &left, &right)) != NULL) {
		if (value > node->value) {
			/*
			 * All nodes in the left sub-tree are less
			 * than the given value. Account them and
			 * inspect the right sub-tree.
			 */
			if (left != NULL)
				count += left->weight;
			count++; /* current node */
			dir = RB_WALK_RIGHT;
		} else {
			/*
			 * The given value is less than or equal
			 * to any value in the right sub-tree.
			 * Inspect the left sub-tree.
			 */
			dir = RB_WALK_LEFT;
		}
	}
	return count;
}

static int
wtree_rank_slow(wtree_t *tree, int value)
{
	int count = 0;;
	struct wtree_iterator it;
	wtree_isearch_lt(tree, value, &it);
	wtree_node_t *node;
	while ((node = wtree_iprev(&it)) != NULL)
		count++;
	return count;
}

static void
check_aug(void)
{
	header();

	int count = 0;		/* actual number of tree nodes */
	int max_count = 3000;	/* max number of tree nodes */
	int max_value = 3000;	/* max node value */
	int remove_prob = 20;	/* chance of removing a node on each iteration */
	int check_count = 100;	/* number of random values to check rank
				 * calculation against */

	wtree_node_t *n;
	wtree_node_t **nodes = calloc(max_count, sizeof(*nodes));

	/* Generate a random tree. */
	wtree_t tree;
	wtree_new(&tree);
	for (int i = 0; i < max_count; i++) {
		if (count > 0 && rand() % 100 < remove_prob) {
			/* Remove a random node. */
			int idx = rand() % count;
			n = nodes[idx];
			nodes[idx] = nodes[--count];
			nodes[count] = NULL;
			wtree_remove(&tree, n);
			free(n);
		}
		/* Insert a node with a random value. */
		n = nodes[count++] = malloc(sizeof(*n));
		n->value = rand() % max_value + 1;
		wtree_insert(&tree, n);
	}

	wtree_selfcheck(&tree);

	for (int i = 0; i < check_count; i++) {
		int value = rand() % (3 * max_value / 2) - max_value / 4;
		fail_unless(wtree_rank(&tree, value) ==
			    wtree_rank_slow(&tree, value));
	}

	for (int i = 0; i < count; i++)
		free(nodes[i]);
	free(nodes);

	footer();
}

int main()
{
	srand(time(NULL));
	check_aug();
	return 0;
}
