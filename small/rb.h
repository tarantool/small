/*-
 *******************************************************************************
 *
 * cpp macro implementation of left-leaning 2-3 red-black trees.  Parent
 * pointers are not used, and color bits are stored in the least significant
 * bit of right-child pointers (if RB_COMPACT is defined), thus making node
 * linkage as compact as is possible for red-black trees.
 *
 * Usage:
 *
 *   #include <stdint.h>
 *   #include <stdbool.h>
 *   #define NDEBUG // (Optional, see assert(3).)
 *   #include <assert.h>
 *   #define RB_COMPACT // (Optional, embed color bits in right-child pointers.)
 *   #define RB_CMP_TREE_ARG // (Optional, passes tree to comparators)
 *   #include <rb.h>
 *   ...
 *
 *******************************************************************************
 */

#ifndef RB_H_
#define	RB_H_

#if 0
__FBSDID("$FreeBSD: head/lib/libc/stdlib/rb.h 204493 2010-02-28 22:57:13Z jasone $");
#endif

#ifdef RB_CMP_TREE_ARG
#define RB_CMP_ARG rbtree,
#else
#define RB_CMP_ARG
#endif

#ifdef RB_COMPACT
/* Node structure. */
#define	rb_node(a_type)							\
struct {								\
    a_type *rbn_left;							\
    a_type *rbn_right_red;						\
}
#else
#define	rb_node(a_type)							\
struct {								\
    a_type *rbn_left;							\
    a_type *rbn_right;							\
    bool rbn_red;							\
}
#endif

/* Root structure. */
#define	rb_tree(a_type, ...)						\
struct {								\
    a_type *rbt_root;							\
    a_type rbt_nil;							\
    __VA_ARGS__								\
}

/* Max length of tree height can be iterated */
#define MAX_ITER_HEIGHT 64

/* Left accessors. */
#define	rbtn_left_get(a_type, a_field, a_node)				\
    ((a_node)->a_field.rbn_left)
#define	rbtn_left_set(a_type, a_field, a_node, a_left) do {		\
    (a_node)->a_field.rbn_left = a_left;				\
} while (0)

#ifdef RB_COMPACT
/* Right accessors. */
#define	rbtn_right_get(a_type, a_field, a_node)				\
    ((a_type *) (((intptr_t) (a_node)->a_field.rbn_right_red)		\
      & ((ssize_t)-2)))
#define	rbtn_right_set(a_type, a_field, a_node, a_right) do {		\
    (a_node)->a_field.rbn_right_red = (a_type *) (((uintptr_t) a_right)	\
      | (((uintptr_t) (a_node)->a_field.rbn_right_red) & ((size_t)1)));	\
} while (0)

/* Color accessors. */
#define	rbtn_red_get(a_type, a_field, a_node)				\
    ((bool) (((uintptr_t) (a_node)->a_field.rbn_right_red)		\
      & ((size_t)1)))
#define	rbtn_color_set(a_type, a_field, a_node, a_red) do {		\
    (a_node)->a_field.rbn_right_red = (a_type *) ((((intptr_t)		\
      (a_node)->a_field.rbn_right_red) & ((ssize_t)-2))			\
      | ((ssize_t)a_red));						\
} while (0)
#define	rbtn_red_set(a_type, a_field, a_node) do {			\
    (a_node)->a_field.rbn_right_red = (a_type *) (((uintptr_t)		\
      (a_node)->a_field.rbn_right_red) | ((size_t)1));			\
} while (0)
#define	rbtn_black_set(a_type, a_field, a_node) do {			\
    (a_node)->a_field.rbn_right_red = (a_type *) (((intptr_t)		\
      (a_node)->a_field.rbn_right_red) & ((ssize_t)-2));		\
} while (0)
#else
/* Right accessors. */
#define	rbtn_right_get(a_type, a_field, a_node)				\
    ((a_node)->a_field.rbn_right)
#define	rbtn_right_set(a_type, a_field, a_node, a_right) do {		\
    (a_node)->a_field.rbn_right = a_right;				\
} while (0)

/* Color accessors. */
#define	rbtn_red_get(a_type, a_field, a_node)				\
    ((a_node)->a_field.rbn_red)
#define	rbtn_color_set(a_type, a_field, a_node, a_red) do {		\
    (a_node)->a_field.rbn_red = (a_red);				\
} while (0)
#define	rbtn_red_set(a_type, a_field, a_node) do {			\
    (a_node)->a_field.rbn_red = true;					\
} while (0)
#define	rbtn_black_set(a_type, a_field, a_node) do {			\
    (a_node)->a_field.rbn_red = false;					\
} while (0)
#endif

/* Node initializer. */
#define	rbt_node_new(a_type, a_field, a_rbt, a_node) do {		\
    rbtn_left_set(a_type, a_field, (a_node), &(a_rbt)->rbt_nil);	\
    rbtn_right_set(a_type, a_field, (a_node), &(a_rbt)->rbt_nil);	\
    rbtn_red_set(a_type, a_field, (a_node));				\
} while (0)

/* Tree initializer. */
#define	rb_new(a_type, a_field, a_rbt) do {				\
    (a_rbt)->rbt_root = &(a_rbt)->rbt_nil;				\
    rbt_node_new(a_type, a_field, a_rbt, &(a_rbt)->rbt_nil);		\
    rbtn_black_set(a_type, a_field, &(a_rbt)->rbt_nil);			\
} while (0)

/* Internal utility macros. */
#define	rbtn_first(a_type, a_field, a_rbt, a_root, r_node) do {		\
    (r_node) = (a_root);						\
    if ((r_node) != &(a_rbt)->rbt_nil) {				\
	for (;								\
	  rbtn_left_get(a_type, a_field, (r_node)) != &(a_rbt)->rbt_nil;\
	  (r_node) = rbtn_left_get(a_type, a_field, (r_node))) {	\
	}								\
    }									\
} while (0)

#define	rbtn_last(a_type, a_field, a_rbt, a_root, r_node) do {		\
    (r_node) = (a_root);						\
    if ((r_node) != &(a_rbt)->rbt_nil) {				\
	for (; rbtn_right_get(a_type, a_field, (r_node)) !=		\
	  &(a_rbt)->rbt_nil; (r_node) = rbtn_right_get(a_type, a_field,	\
	  (r_node))) {							\
	}								\
    }									\
} while (0)

#define	rbtn_rotate_left(a_type, a_field, a_node, r_node) do {		\
    (r_node) = rbtn_right_get(a_type, a_field, (a_node));		\
    rbtn_right_set(a_type, a_field, (a_node),				\
      rbtn_left_get(a_type, a_field, (r_node)));			\
    rbtn_left_set(a_type, a_field, (r_node), (a_node));			\
} while (0)

#define	rbtn_rotate_right(a_type, a_field, a_node, r_node) do {		\
    (r_node) = rbtn_left_get(a_type, a_field, (a_node));		\
    rbtn_left_set(a_type, a_field, (a_node),				\
      rbtn_right_get(a_type, a_field, (r_node)));			\
    rbtn_right_set(a_type, a_field, (r_node), (a_node));		\
} while (0)

#define rbtn_iter_go_left_down(a_type, a_field, rbtree, node, it) do {	\
    a_type *cur = (node);						\
    do {								\
	assert((it)->count < MAX_ITER_HEIGHT);				\
	(it)->path[(it)->count++] = cur;				\
	cur = rbtn_left_get(a_type, a_field, (cur));			\
    } while (cur != &(rbtree)->rbt_nil);				\
} while (0)

#define rbtn_iter_go_right_up(a_type, a_field, rbtree, it) do {		\
    while(--(it)->count > 0) {						\
	if (rbtn_right_get(a_type, a_field,				\
			  (it)->path[(it)->count - 1]) !=		\
			  (it)->path[(it)->count]) {			\
	    break;							\
	}								\
    }									\
} while (0)

#define rbtn_iter_go_right_down(a_type, a_field, rbtree, node, it) do {	\
    a_type *cur = (node);						\
    do{									\
	assert((it)->count < MAX_ITER_HEIGHT);				\
	(it)->path[(it)->count++] = cur;				\
	cur = rbtn_right_get(a_type, a_field, (cur));			\
    } while (cur != &(rbtree)->rbt_nil);				\
} while (0)

#define rbtn_iter_go_left_up(a_type, a_field, rbtree, it) do {		\
    while(--(it)->count > 0) {						\
	if (rbtn_left_get(a_type, a_field,				\
			 (it)->path[(it)->count - 1]) !=		\
			 (it)->path[(it)->count]) {			\
	    break;							\
	}								\
    }									\
} while (0)								\

/*
 * The rb_proto() macro generates function prototypes that correspond to the
 * functions generated by an equivalently parameterized call to rb_gen().
 */

#define	rb_proto_ext_key(a_attr, a_prefix, a_rbt_type, a_type, a_key)	\
struct a_prefix##iterator;						\
a_attr void								\
a_prefix##new(a_rbt_type *rbtree);					\
a_attr a_type *								\
a_prefix##first(a_rbt_type *rbtree);					\
a_attr a_type *								\
a_prefix##last(a_rbt_type *rbtree);					\
a_attr a_type *								\
a_prefix##next(a_rbt_type *rbtree, a_type *node);			\
a_attr a_type *								\
a_prefix##prev(a_rbt_type *rbtree, a_type *node);			\
a_attr a_type *								\
a_prefix##search(a_rbt_type *rbtree, a_key key);			\
a_attr a_type *								\
a_prefix##psearch(a_rbt_type *rbtree, a_key key);			\
a_attr a_type *								\
a_prefix##nsearch(a_rbt_type *rbtree, a_key key);			\
a_attr void								\
a_prefix##insert(a_rbt_type *rbtree, a_type *node);			\
a_attr void								\
a_prefix##remove(a_rbt_type *rbtree, a_type *node);			\
a_attr a_type *								\
a_prefix##iterator_get(struct a_prefix##iterator *it);			\
a_attr bool								\
a_prefix##icreate(a_rbt_type *rbtree, a_type *node,			\
		  struct a_prefix##iterator *it);			\
a_attr void								\
a_prefix##ifirst(a_rbt_type *rbtree, struct a_prefix##iterator *it);	\
a_attr void								\
a_prefix##ilast(a_rbt_type *rbtree, struct a_prefix##iterator *it);	\
a_attr a_type *								\
a_prefix##inext(a_rbt_type *rbtree, struct a_prefix##iterator *it);	\
a_attr a_type *								\
a_prefix##iprev(a_rbt_type *rbtree, struct a_prefix##iterator *it);	\
a_attr bool								\
a_prefix##isearch(a_rbt_type *rbtree, a_key key,			\
		  struct a_prefix##iterator *it);			\
a_attr void								\
a_prefix##isearch_le(a_rbt_type *rbtree, a_key key,			\
		     struct a_prefix##iterator *it);			\
a_attr void								\
a_prefix##isearch_ge(a_rbt_type *rbtree, a_key key,			\
		    struct a_prefix##iterator *it);			\
a_attr void								\
a_prefix##isearch_lt(a_rbt_type *rbtree, a_key key,			\
		     struct a_prefix##iterator *it);			\
a_attr void								\
a_prefix##isearch_gt(a_rbt_type *rbtree, a_key key,			\
		     struct a_prefix##iterator *it);			\
a_attr a_type *								\
a_prefix##iter(a_rbt_type *rbtree, a_type *start, a_type *(*cb)(	\
  a_rbt_type *, a_type *, void *), void *arg);				\
a_attr a_type *								\
a_prefix##reverse_iter(a_rbt_type *rbtree, a_type *start,		\
  a_type *(*cb)(a_rbt_type *, a_type *, void *), void *arg);
#define	rb_proto(a_attr, a_prefix, a_rbt_type, a_type)			\
rb_proto_ext_key(a_attr, a_prefix, a_rbt_type, a_type, a_type *)

/*
 * The rb_gen() macro generates a type-specific red-black tree implementation,
 * based on the above cpp macros.
 *
 * Arguments:
 *
 *   a_attr    : Function attribute for generated functions (ex: static).
 *   a_prefix  : Prefix for generated functions (ex: ex_).
 *   a_rb_type : Type for red-black tree data structure (ex: ex_t).
 *   a_type    : Type for red-black tree node data structure (ex: ex_node_t).
 *   a_field   : Name of red-black tree node linkage (ex: ex_link).
 *   a_cmp     : Node comparison function name, with the following prototype:
 *                 int (a_cmp *)(a_type *a_node, a_type *a_other);
 *                                       ^^^^^^
 *                                    or a_key
 *               Interpretation of comparision function return values:
 *                 -1 : a_node <  a_other
 *                  0 : a_node == a_other
 *                  1 : a_node >  a_other
 *               In all cases, the a_node or a_key macro argument is the first
 *               argument to the comparison function, which makes it possible
 *               to write comparison functions that treat the first argument
 *               specially.
 *
 * Assuming the following setup:
 *
 *   typedef struct ex_node_s ex_node_t;
 *   struct ex_node_s {
 *       rb_node(ex_node_t) ex_link;
 *   };
 *   typedef rb_tree(ex_node_t) ex_t;
 *   rb_gen(static, ex_, ex_t, ex_node_t, ex_link, ex_cmp)
 *
 * The following API is generated:
 *
 *   static void
 *   ex_new(ex_t *tree);
 *       Description: Initialize a red-black tree structure.
 *       Args:
 *         tree: Pointer to an uninitialized red-black tree object.
 *
 *   static ex_node_t *
 *   ex_first(ex_t *tree);
 *   static ex_node_t *
 *   ex_last(ex_t *tree);
 *       Description: Get the first/last node in tree.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *       Ret: First/last node in tree, or NULL if tree is empty.
 *
 *   static ex_node_t *
 *   ex_next(ex_t *tree, ex_node_t *node);
 *   static ex_node_t *
 *   ex_prev(ex_t *tree, ex_node_t *node);
 *       Description: Get node's successor/predecessor.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         node: A node in tree.
 *       Ret: node's successor/predecessor in tree, or NULL if node is
 *            last/first.
 *
 *   static ex_node_t *
 *   ex_search(ex_t *tree, ex_node_t *key);
 *       Description: Search for node that matches key.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         key : Search key.
 *       Ret: Node in tree that matches key, or NULL if no match.
 *
 *   Let's explain next searching functions with example
 *   Assume having following set of the keys:
 *   ((1,2), (1,3), (2,1), (2,2), (2,3), (3,1), (3,2))
 *   Comparison function if natural: first comparing first index, then second.
 *   static ex_node_t *
 *   ex_nsearch(ex_t *tree, ex_node_t *key);
 *   static ex_node_t *
 *   ex_psearch(ex_t *tree, ex_node_t *key);
 *       Description: If match most/least of matching.
 *                    If no match is found, return what would be key's
 *                    successor/predecessor, were key in tree.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         key : Search key.
 *       Ret: Node in tree that matches key, or if no match, hypothetical node's
 *            successor/predecessor (NULL if no successor/predecessor).
 *   In out example:
 *   _nsearch2)=(2,3) _nsearch(0)=(1,2) _nsearch(4)=nil
 *   _psearch(2)=(2,1) _psearch(0)=nil   _psearch(4)=(3,2)
 *
 *   static void
 *   ex_insert(ex_t *tree, ex_node_t *node);
 *       Description: Insert node into tree.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         node: Node to be inserted into tree.
 *
 *   static void
 *   ex_remove(ex_t *tree, ex_node_t *node);
 *       Description: Remove node from tree.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         node: Node in tree to be removed.
 *
 *   static ex_node_t *
 *   ex_iter(ex_t *tree, ex_node_t *start, ex_node_t *(*cb)(ex_t *,
 *     ex_node_t *, void *), void *arg);
 *   static ex_node_t *
 *   ex_reverse_iter(ex_t *tree, ex_node_t *start, ex_node *(*cb)(ex_t *,
 *     ex_node_t *, void *), void *arg);
 *       Description: Iterate forward/backward over tree, starting at node.  If
 *                    tree is modified, iteration must be immediately
 *                    terminated by the callback function that causes the
 *                    modification.
 *       Args:
 *         tree : Pointer to an initialized red-black tree object.
 *         start: Node at which to start iteration, or NULL to start at
 *                first/last node.
 *         cb   : Callback function, which is called for each node during
 *                iteration.  Under normal circumstances the callback function
 *                should return NULL, which causes iteration to continue.  If a
 *                callback function returns non-NULL, iteration is immediately
 *                terminated and the non-NULL return value is returned by the
 *                iterator.  This is useful for re-starting iteration after
 *                modifying tree.
 *         arg  : Opaque pointer passed to cb().
 *       Ret: NULL if iteration completed, or the non-NULL callback return value
 *            that caused termination of the iteration.
 *
 *      Also will be generated following iterator API:
 *
 *   struct ex_iterator;
 *       Description: Struct for iteration over the tree.
 *
 *   static ex_node *
 *   ex_iterator_get(ex_iterator *it)
 *       Description: Get value iterator points to.
 *       Args:
 *         it   : Pointer to initialized iterator
 *       Ret: NULL if iterator points to nothing, else value.
 *   static bool
 *   ex_icreate(ex_t *tree, ex_node_t *node, ex_iterator *it);
 *       Description: Create the iterator that corresponds to node.
 *       Args:
 *         tree : Pointer to an initialized red-black tree object.
 *         node : Pointer to a node at which start iteration.
 *         it   : Pointer to an uninitialized iterator.
 *       Ret: true if found given node, false if not.
 *
 *   static void
 *   ex_ifirst(ex_t *tree, ex_iterator *it);
 *       Description: Locate iterator to the first node of the tree.
 *       Args:
 *         tree : Pointer to an initialized red-black tree object.
 *         it   : Pointer to an uninitialized iterator.
 *
 *   static void
 *   ex_ilast(ex_t *tree, ex_iterator *it);
 *       Description: Locate iterator to the last node of the tree.
 *       Args:
 *         tree : Pointer to an initialized red-black tree object.
 *         it   : Pointer to an uninitialized iterator.
 *
 *   static ex_node_t *
 *   ex_inext(ex_t *tree, ex_iterator *it);
 *       Description: Iterate to the next node. Change iterator.
 *       Args:
 *         tree : Pointer to an initialized red-black tree object.
 *         it   : Pointer to an initialized iterator.
 *       Ret: NULL if iteration ends; non-NULL node,
 *            at which current iterator points.
 *
 *   static ex_node_t *
 *   ex_iprev(ex_t *tree, ex_iterator *it);
 *       Description: Iterate to the previous node. Change iterator.
 *       Args:
 *         tree : Pointer to an initialized red-black tree object.
 *         it   : Pointer to an initialized iterator.
 *       Ret: NULL if iteration ends; non-NULL node,
 *            at which current iterator points
 *
 *   static bool
 *   ex_isearch(ex_t *tree, ex_key key, ex_iterator *it);
 *       Description: Search for node that matches key and
 *                    set \a it pointed to this node.
 *       Args:
 *         tree : Pointer to an initialized red-black tree object.
 *         key  : key to find a node at which iterator would point.
 *         it   : Pointer to an uninitialized iterator.
 *       Ret: true if found, false if not.
 *
 *   static void
 *   ex_isearch_le(ex_t *tree, ex_node_t *key, ex_iterator *it);
 *       Description: Search for the most node that Less of Equals to key,
 *                    set \a it pointed to this node.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         key : Search key.
 *         it  : Pointer to an uninitialized iterator.
 *
 *   static void
 *   ex_isearch_ge(ex_t *tree, ex_node_t *key, ex_iterator *it);
 *       Description: Search for least node that Greater of Equals to key,
 *                    set \a it pointed to this node.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         key : Search key.
 *         it  : Pointer to an uninitialized iterator.
 *
 *   static void
 *   ex_isearch_lt(ex_t *tree, ex_node_t *key, ex_iterator *it);
 *       Description: Search for most node that Less Than key,
 *                    set \a it pointed to this node.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         key : Search key.
 *         it  : Pointer to an uninitialized iterator.
 *
 *   static void
 *   ex_isearch_gt(ex_t *tree, ex_node_t *key, ex_iterator *it);
 *       Description: Search for least node that Greater Than key,
 *                    set \a it pointed to this node.
 *       Args:
 *         tree: Pointer to an initialized red-black tree object.
 *         key : Search key.
 *         it  : Pointer to an uninitialized iterator.
 *
 * There is also an extended 'rb_gen_ext_key' macro that allows to generate rb
 * code with specified key type and comparator for [p|n]search and isearch*
 * methods. So using this macro instead of 'rb_gen':
 *
 *   ...
 *   int (ex_key_cmp *)(int key, ex_node *node);
 *   ...
 *   rb_gen_ext_key(static, ex_, ex_t, ex_node_t, ex_link, ex_cmp,
 *                  int, ex_key_cmp)
 *
 * Will generate same code as 'rb_gen' macro except for 8 functions:
 *
 *   static ex_node_t *
 *   ex_search(ex_t *tree, int key);
 *
 *   static ex_node_t *
 *   ex_nsearch(ex_t *tree, int key);
 *
 *   static ex_node_t *
 *   ex_psearch(ex_t *tree, int key);
 *
 *   static bool
 *   ex_isearch(ex_t *tree, imt key, ex_iterator *it);
 *
 *   static void
 *   ex_isearch_le(ex_t *tree, imt key, ex_iterator *it);
 *
 *   static void
 *   ex_isearch_ge(ex_t *tree, imt key, ex_iterator *it);
 *
 *   static void
 *   ex_isearch_lt(ex_t *tree, imt key, ex_iterator *it);
 *
 *   static void
 *   ex_isearch_gt(ex_t *tree, imt key, ex_iterator *it);
 *
 * One can also used 'rb_proto_ext_key' macro to generate a declaration of
 * all methods with that kind of search methods. Comparing to 'rb_gen', this
 * macro has one additional argument - type of key.
 *
 */
#define	rb_gen_ext_key(a_attr, a_prefix, a_rbt_type, a_type, a_field,	\
		       a_cmp, a_key, a_cmp_key)				\
struct a_prefix##iterator {                                             \
        a_type *path[MAX_ITER_HEIGHT];                                  \
        uint32_t count;                                                 \
};                                                                      \
a_attr void								\
a_prefix##new(a_rbt_type *rbtree) {					\
    rb_new(a_type, a_field, rbtree);					\
}									\
a_attr a_type *								\
a_prefix##first(a_rbt_type *rbtree) {					\
    a_type *ret;							\
    rbtn_first(a_type, a_field, rbtree, rbtree->rbt_root, ret);		\
    if (ret == &rbtree->rbt_nil) {					\
	ret = NULL;							\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_prefix##last(a_rbt_type *rbtree) {					\
    a_type *ret;							\
    rbtn_last(a_type, a_field, rbtree, rbtree->rbt_root, ret);		\
    if (ret == &rbtree->rbt_nil) {					\
	ret = NULL;							\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_prefix##next(a_rbt_type *rbtree, a_type *node) {			\
    a_type *ret;							\
    if (rbtn_right_get(a_type, a_field, node) != &rbtree->rbt_nil) {	\
	rbtn_first(a_type, a_field, rbtree, rbtn_right_get(a_type,	\
	  a_field, node), ret);						\
    } else {								\
	a_type *tnode = rbtree->rbt_root;				\
	assert(tnode != &rbtree->rbt_nil);				\
	ret = &rbtree->rbt_nil;						\
	while (true) {							\
	    int cmp = a_cmp(RB_CMP_ARG node, tnode);			\
	    if (cmp < 0) {						\
		ret = tnode;						\
		tnode = rbtn_left_get(a_type, a_field, tnode);		\
	    } else if (cmp > 0) {					\
		tnode = rbtn_right_get(a_type, a_field, tnode);		\
	    } else {							\
		break;							\
	    }								\
	    assert(tnode != &rbtree->rbt_nil);				\
	}								\
    }									\
    if (ret == &rbtree->rbt_nil) {					\
	ret = (NULL);							\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_prefix##prev(a_rbt_type *rbtree, a_type *node) {			\
    a_type *ret;							\
    if (rbtn_left_get(a_type, a_field, node) != &rbtree->rbt_nil) {	\
	rbtn_last(a_type, a_field, rbtree, rbtn_left_get(a_type,	\
	  a_field, node), ret);						\
    } else {								\
	a_type *tnode = rbtree->rbt_root;				\
	assert(tnode != &rbtree->rbt_nil);				\
	ret = &rbtree->rbt_nil;						\
	while (true) {							\
	    int cmp = a_cmp(RB_CMP_ARG node, tnode);			\
	    if (cmp < 0) {						\
		tnode = rbtn_left_get(a_type, a_field, tnode);		\
	    } else if (cmp > 0) {					\
		ret = tnode;						\
		tnode = rbtn_right_get(a_type, a_field, tnode);		\
	    } else {							\
		break;							\
	    }								\
	    assert(tnode != &rbtree->rbt_nil);				\
	}								\
    }									\
    if (ret == &rbtree->rbt_nil) {					\
	ret = (NULL);							\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_prefix##search(a_rbt_type *rbtree, a_key key) {			\
    a_type *ret;							\
    int cmp;								\
    ret = rbtree->rbt_root;						\
    while (ret != &rbtree->rbt_nil					\
      && (cmp = a_cmp_key(RB_CMP_ARG key, ret)) != 0) {			\
	if (cmp < 0) {							\
	    ret = rbtn_left_get(a_type, a_field, ret);			\
	} else {							\
	    ret = rbtn_right_get(a_type, a_field, ret);			\
	}								\
    }									\
    if (ret == &rbtree->rbt_nil) {					\
	ret = (NULL);							\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_prefix##nsearch(a_rbt_type *rbtree, a_key key) {			\
    a_type *ret, *next;							\
    a_type *tnode = rbtree->rbt_root;					\
    ret = &rbtree->rbt_nil;						\
    next = NULL;							\
    while (tnode != &rbtree->rbt_nil) {					\
	int cmp = a_cmp_key(RB_CMP_ARG key, tnode);			\
	if (cmp < 0) {							\
	    next = tnode;						\
	    tnode = rbtn_left_get(a_type, a_field, tnode);		\
	} else if (cmp > 0) {						\
	    tnode = rbtn_right_get(a_type, a_field, tnode);		\
	} else {							\
	    ret = tnode;						\
	    tnode = rbtn_right_get(a_type, a_field, tnode);		\
	}								\
    }									\
    if (ret == &rbtree->rbt_nil) {					\
	return next;							\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_prefix##psearch(a_rbt_type *rbtree, a_key key) {			\
    a_type *ret, *prev;							\
    a_type *tnode = rbtree->rbt_root;					\
    ret = &rbtree->rbt_nil;						\
    prev = NULL;							\
    while (tnode != &rbtree->rbt_nil) {					\
	int cmp = a_cmp_key(RB_CMP_ARG key, tnode);			\
	if (cmp < 0) {							\
	    tnode = rbtn_left_get(a_type, a_field, tnode);		\
	} else if (cmp > 0) {						\
	    prev = tnode;						\
	    tnode = rbtn_right_get(a_type, a_field, tnode);		\
	} else {							\
	    ret = tnode;						\
	    tnode = rbtn_left_get(a_type, a_field, tnode);		\
	}								\
    }									\
    if (ret == &rbtree->rbt_nil) {					\
	return prev;							\
    }									\
    return (ret);							\
}									\
a_attr void								\
a_prefix##insert(a_rbt_type *rbtree, a_type *node) {			\
    struct {								\
	a_type *node;							\
	int cmp;							\
    } path[sizeof(void *) << 4], *pathp;				\
    rbt_node_new(a_type, a_field, rbtree, node);			\
    /* Wind. */								\
    path->node = rbtree->rbt_root;					\
    for (pathp = path; pathp->node != &rbtree->rbt_nil; pathp++) {	\
	int cmp = pathp->cmp = a_cmp(RB_CMP_ARG node, pathp->node);	\
	assert(cmp != 0);						\
	if (cmp < 0) {							\
	    pathp[1].node = rbtn_left_get(a_type, a_field,		\
	      pathp->node);						\
	} else {							\
	    pathp[1].node = rbtn_right_get(a_type, a_field,		\
	      pathp->node);						\
	}								\
    }									\
    pathp->node = node;							\
    /* Unwind. */							\
    for (pathp--; (uintptr_t)pathp >= (uintptr_t)path; pathp--) {	\
	a_type *cnode = pathp->node;					\
	if (pathp->cmp < 0) {						\
	    a_type *left = pathp[1].node;				\
	    rbtn_left_set(a_type, a_field, cnode, left);		\
	    if (rbtn_red_get(a_type, a_field, left)) {			\
		a_type *leftleft = rbtn_left_get(a_type, a_field, left);\
		if (rbtn_red_get(a_type, a_field, leftleft)) {		\
		    /* Fix up 4-node. */				\
		    a_type *tnode;					\
		    rbtn_black_set(a_type, a_field, leftleft);		\
		    rbtn_rotate_right(a_type, a_field, cnode, tnode);	\
		    cnode = tnode;					\
		}							\
	    } else {							\
		return;							\
	    }								\
	} else {							\
	    a_type *right = pathp[1].node;				\
	    rbtn_right_set(a_type, a_field, cnode, right);		\
	    if (rbtn_red_get(a_type, a_field, right)) {			\
		a_type *left = rbtn_left_get(a_type, a_field, cnode);	\
		if (rbtn_red_get(a_type, a_field, left)) {		\
		    /* Split 4-node. */					\
		    rbtn_black_set(a_type, a_field, left);		\
		    rbtn_black_set(a_type, a_field, right);		\
		    rbtn_red_set(a_type, a_field, cnode);		\
		} else {						\
		    /* Lean left. */					\
		    a_type *tnode;					\
		    bool tred = rbtn_red_get(a_type, a_field, cnode);	\
		    rbtn_rotate_left(a_type, a_field, cnode, tnode);	\
		    rbtn_color_set(a_type, a_field, tnode, tred);	\
		    rbtn_red_set(a_type, a_field, cnode);		\
		    cnode = tnode;					\
		}							\
	    } else {							\
		return;							\
	    }								\
	}								\
	pathp->node = cnode;						\
    }									\
    /* Set root, and make it black. */					\
    rbtree->rbt_root = path->node;					\
    rbtn_black_set(a_type, a_field, rbtree->rbt_root);			\
}									\
a_attr void								\
a_prefix##remove(a_rbt_type *rbtree, a_type *node) {			\
    struct {								\
	a_type *node;							\
	int cmp;							\
    } *pathp, *nodep, path[sizeof(void *) << 4];			\
    /* Wind. */								\
    nodep = NULL; /* Silence compiler warning. */			\
    path->node = rbtree->rbt_root;					\
    for (pathp = path; pathp->node != &rbtree->rbt_nil; pathp++) {	\
	int cmp = pathp->cmp = a_cmp(RB_CMP_ARG node, pathp->node);	\
	if (cmp < 0) {							\
	    pathp[1].node = rbtn_left_get(a_type, a_field,		\
	      pathp->node);						\
	} else {							\
	    pathp[1].node = rbtn_right_get(a_type, a_field,		\
	      pathp->node);						\
	    if (cmp == 0) {						\
	        /* Find node's successor, in preparation for swap. */	\
		pathp->cmp = 1;						\
		nodep = pathp;						\
		for (pathp++; pathp->node != &rbtree->rbt_nil;		\
		  pathp++) {						\
		    pathp->cmp = -1;					\
		    pathp[1].node = rbtn_left_get(a_type, a_field,	\
		      pathp->node);					\
		}							\
		break;							\
	    }								\
	}								\
    }									\
    assert(nodep->node == node);					\
    pathp--;								\
    if (pathp->node != node) {						\
	/* Swap node with its successor. */				\
	bool tred = rbtn_red_get(a_type, a_field, pathp->node);		\
	rbtn_color_set(a_type, a_field, pathp->node,			\
	  rbtn_red_get(a_type, a_field, node));				\
	rbtn_left_set(a_type, a_field, pathp->node,			\
	  rbtn_left_get(a_type, a_field, node));			\
	/* If node's successor is its right child, the following code */\
	/* will do the wrong thing for the right child pointer.       */\
	/* However, it doesn't matter, because the pointer will be    */\
	/* properly set when the successor is pruned.                 */\
	rbtn_right_set(a_type, a_field, pathp->node,			\
	  rbtn_right_get(a_type, a_field, node));			\
	rbtn_color_set(a_type, a_field, node, tred);			\
	/* The pruned leaf node's child pointers are never accessed   */\
	/* again, so don't bother setting them to nil.                */\
	nodep->node = pathp->node;					\
	pathp->node = node;						\
	if (nodep == path) {						\
	    rbtree->rbt_root = nodep->node;				\
	} else {							\
	    if (nodep[-1].cmp < 0) {					\
		rbtn_left_set(a_type, a_field, nodep[-1].node,		\
		  nodep->node);						\
	    } else {							\
		rbtn_right_set(a_type, a_field, nodep[-1].node,		\
		  nodep->node);						\
	    }								\
	}								\
    } else {								\
	a_type *left = rbtn_left_get(a_type, a_field, node);		\
	if (left != &rbtree->rbt_nil) {					\
	    /* node has no successor, but it has a left child.        */\
	    /* Splice node out, without losing the left child.        */\
	    assert(rbtn_red_get(a_type, a_field, node) == false);	\
	    assert(rbtn_red_get(a_type, a_field, left));		\
	    rbtn_black_set(a_type, a_field, left);			\
	    if (pathp == path) {					\
		rbtree->rbt_root = left;				\
	    } else {							\
		if (pathp[-1].cmp < 0) {				\
		    rbtn_left_set(a_type, a_field, pathp[-1].node,	\
		      left);						\
		} else {						\
		    rbtn_right_set(a_type, a_field, pathp[-1].node,	\
		      left);						\
		}							\
	    }								\
	    return;							\
	} else if (pathp == path) {					\
	    /* The tree only contained one node. */			\
	    rbtree->rbt_root = &rbtree->rbt_nil;			\
	    return;							\
	}								\
    }									\
    if (rbtn_red_get(a_type, a_field, pathp->node)) {			\
	/* Prune red node, which requires no fixup. */			\
	assert(pathp[-1].cmp < 0);					\
	rbtn_left_set(a_type, a_field, pathp[-1].node,			\
	  &rbtree->rbt_nil);						\
	return;								\
    }									\
    /* The node to be pruned is black, so unwind until balance is     */\
    /* restored.                                                      */\
    pathp->node = &rbtree->rbt_nil;					\
    for (pathp--; (uintptr_t)pathp >= (uintptr_t)path; pathp--) {	\
	assert(pathp->cmp != 0);					\
	if (pathp->cmp < 0) {						\
	    rbtn_left_set(a_type, a_field, pathp->node,			\
	      pathp[1].node);						\
	    assert(rbtn_red_get(a_type, a_field, pathp[1].node)		\
	      == false);						\
	    if (rbtn_red_get(a_type, a_field, pathp->node)) {		\
		a_type *right = rbtn_right_get(a_type, a_field,		\
		  pathp->node);						\
		a_type *rightleft = rbtn_left_get(a_type, a_field,	\
		  right);						\
		a_type *tnode;						\
		if (rbtn_red_get(a_type, a_field, rightleft)) {		\
		    /* In the following diagrams, ||, //, and \\      */\
		    /* indicate the path to the removed node.         */\
		    /*                                                */\
		    /*      ||                                        */\
		    /*    pathp(r)                                    */\
		    /*  //        \                                   */\
		    /* (b)        (b)                                 */\
		    /*           /                                    */\
		    /*          (r)                                   */\
		    /*                                                */\
		    rbtn_black_set(a_type, a_field, pathp->node);	\
		    rbtn_rotate_right(a_type, a_field, right, tnode);	\
		    rbtn_right_set(a_type, a_field, pathp->node, tnode);\
		    rbtn_rotate_left(a_type, a_field, pathp->node,	\
		      tnode);						\
		} else {						\
		    /*      ||                                        */\
		    /*    pathp(r)                                    */\
		    /*  //        \                                   */\
		    /* (b)        (b)                                 */\
		    /*           /                                    */\
		    /*          (b)                                   */\
		    /*                                                */\
		    rbtn_rotate_left(a_type, a_field, pathp->node,	\
		      tnode);						\
		}							\
		/* Balance restored, but rotation modified subtree    */\
		/* root.                                              */\
		assert((uintptr_t)pathp > (uintptr_t)path);		\
		if (pathp[-1].cmp < 0) {				\
		    rbtn_left_set(a_type, a_field, pathp[-1].node,	\
		      tnode);						\
		} else {						\
		    rbtn_right_set(a_type, a_field, pathp[-1].node,	\
		      tnode);						\
		}							\
		return;							\
	    } else {							\
		a_type *right = rbtn_right_get(a_type, a_field,		\
		  pathp->node);						\
		a_type *rightleft = rbtn_left_get(a_type, a_field,	\
		  right);						\
		if (rbtn_red_get(a_type, a_field, rightleft)) {		\
		    /*      ||                                        */\
		    /*    pathp(b)                                    */\
		    /*  //        \                                   */\
		    /* (b)        (b)                                 */\
		    /*           /                                    */\
		    /*          (r)                                   */\
		    a_type *tnode;					\
		    rbtn_black_set(a_type, a_field, rightleft);		\
		    rbtn_rotate_right(a_type, a_field, right, tnode);	\
		    rbtn_right_set(a_type, a_field, pathp->node, tnode);\
		    rbtn_rotate_left(a_type, a_field, pathp->node,	\
		      tnode);						\
		    /* Balance restored, but rotation modified        */\
		    /* subree root, which may actually be the tree    */\
		    /* root.                                          */\
		    if (pathp == path) {				\
			/* Set root. */					\
			rbtree->rbt_root = tnode;			\
		    } else {						\
			if (pathp[-1].cmp < 0) {			\
			    rbtn_left_set(a_type, a_field,		\
			      pathp[-1].node, tnode);			\
			} else {					\
			    rbtn_right_set(a_type, a_field,		\
			      pathp[-1].node, tnode);			\
			}						\
		    }							\
		    return;						\
		} else {						\
		    /*      ||                                        */\
		    /*    pathp(b)                                    */\
		    /*  //        \                                   */\
		    /* (b)        (b)                                 */\
		    /*           /                                    */\
		    /*          (b)                                   */\
		    a_type *tnode;					\
		    rbtn_red_set(a_type, a_field, pathp->node);		\
		    rbtn_rotate_left(a_type, a_field, pathp->node,	\
		      tnode);						\
		    pathp->node = tnode;				\
		}							\
	    }								\
	} else {							\
	    a_type *left;						\
	    rbtn_right_set(a_type, a_field, pathp->node,		\
	      pathp[1].node);						\
	    left = rbtn_left_get(a_type, a_field, pathp->node);		\
	    if (rbtn_red_get(a_type, a_field, left)) {			\
		a_type *tnode;						\
		a_type *leftright = rbtn_right_get(a_type, a_field,	\
		  left);						\
		a_type *leftrightleft = rbtn_left_get(a_type, a_field,	\
		  leftright);						\
		if (rbtn_red_get(a_type, a_field, leftrightleft)) {	\
		    /*      ||                                        */\
		    /*    pathp(b)                                    */\
		    /*   /        \\                                  */\
		    /* (r)        (b)                                 */\
		    /*   \                                            */\
		    /*   (b)                                          */\
		    /*   /                                            */\
		    /* (r)                                            */\
		    a_type *unode;					\
		    rbtn_black_set(a_type, a_field, leftrightleft);	\
		    rbtn_rotate_right(a_type, a_field, pathp->node,	\
		      unode);						\
		    rbtn_rotate_right(a_type, a_field, pathp->node,	\
		      tnode);						\
		    rbtn_right_set(a_type, a_field, unode, tnode);	\
		    rbtn_rotate_left(a_type, a_field, unode, tnode);	\
		} else {						\
		    /*      ||                                        */\
		    /*    pathp(b)                                    */\
		    /*   /        \\                                  */\
		    /* (r)        (b)                                 */\
		    /*   \                                            */\
		    /*   (b)                                          */\
		    /*   /                                            */\
		    /* (b)                                            */\
		    assert(leftright != &rbtree->rbt_nil);		\
		    rbtn_red_set(a_type, a_field, leftright);		\
		    rbtn_rotate_right(a_type, a_field, pathp->node,	\
		      tnode);						\
		    rbtn_black_set(a_type, a_field, tnode);		\
		}							\
		/* Balance restored, but rotation modified subtree    */\
		/* root, which may actually be the tree root.         */\
		if (pathp == path) {					\
		    /* Set root. */					\
		    rbtree->rbt_root = tnode;				\
		} else {						\
		    if (pathp[-1].cmp < 0) {				\
			rbtn_left_set(a_type, a_field, pathp[-1].node,	\
			  tnode);					\
		    } else {						\
			rbtn_right_set(a_type, a_field, pathp[-1].node,	\
			  tnode);					\
		    }							\
		}							\
		return;							\
	    } else if (rbtn_red_get(a_type, a_field, pathp->node)) {	\
		a_type *leftleft = rbtn_left_get(a_type, a_field, left);\
		if (rbtn_red_get(a_type, a_field, leftleft)) {		\
		    /*        ||                                      */\
		    /*      pathp(r)                                  */\
		    /*     /        \\                                */\
		    /*   (b)        (b)                               */\
		    /*   /                                            */\
		    /* (r)                                            */\
		    a_type *tnode;					\
		    rbtn_black_set(a_type, a_field, pathp->node);	\
		    rbtn_red_set(a_type, a_field, left);		\
		    rbtn_black_set(a_type, a_field, leftleft);		\
		    rbtn_rotate_right(a_type, a_field, pathp->node,	\
		      tnode);						\
		    /* Balance restored, but rotation modified        */\
		    /* subtree root.                                  */\
		    assert((uintptr_t)pathp > (uintptr_t)path);		\
		    if (pathp[-1].cmp < 0) {				\
			rbtn_left_set(a_type, a_field, pathp[-1].node,	\
			  tnode);					\
		    } else {						\
			rbtn_right_set(a_type, a_field, pathp[-1].node,	\
			  tnode);					\
		    }							\
		    return;						\
		} else {						\
		    /*        ||                                      */\
		    /*      pathp(r)                                  */\
		    /*     /        \\                                */\
		    /*   (b)        (b)                               */\
		    /*   /                                            */\
		    /* (b)                                            */\
		    rbtn_red_set(a_type, a_field, left);		\
		    rbtn_black_set(a_type, a_field, pathp->node);	\
		    /* Balance restored. */				\
		    return;						\
		}							\
	    } else {							\
		a_type *leftleft = rbtn_left_get(a_type, a_field, left);\
		if (rbtn_red_get(a_type, a_field, leftleft)) {		\
		    /*               ||                               */\
		    /*             pathp(b)                           */\
		    /*            /        \\                         */\
		    /*          (b)        (b)                        */\
		    /*          /                                     */\
		    /*        (r)                                     */\
		    a_type *tnode;					\
		    rbtn_black_set(a_type, a_field, leftleft);		\
		    rbtn_rotate_right(a_type, a_field, pathp->node,	\
		      tnode);						\
		    /* Balance restored, but rotation modified        */\
		    /* subtree root, which may actually be the tree   */\
		    /* root.                                          */\
		    if (pathp == path) {				\
			/* Set root. */					\
			rbtree->rbt_root = tnode;			\
		    } else {						\
			if (pathp[-1].cmp < 0) {			\
			    rbtn_left_set(a_type, a_field,		\
			      pathp[-1].node, tnode);			\
			} else {					\
			    rbtn_right_set(a_type, a_field,		\
			      pathp[-1].node, tnode);			\
			}						\
		    }							\
		    return;						\
		} else {						\
		    /*               ||                               */\
		    /*             pathp(b)                           */\
		    /*            /        \\                         */\
		    /*          (b)        (b)                        */\
		    /*          /                                     */\
		    /*        (b)                                     */\
		    rbtn_red_set(a_type, a_field, left);		\
		}							\
	    }								\
	}								\
    }									\
    /* Set root. */							\
    rbtree->rbt_root = path->node;					\
    assert(rbtn_red_get(a_type, a_field, rbtree->rbt_root) == false);	\
}									\
a_attr a_type *								\
a_prefix##iterator_get(struct a_prefix##iterator *it)			\
{									\
    if (it->count <= 0) {						\
	return NULL;							\
    }									\
    return it->path[it->count - 1];					\
}									\
a_attr bool								\
a_prefix##icreate(a_rbt_type *rbtree, a_type *node,			\
	  struct a_prefix##iterator *it)				\
{									\
    assert(rbtree);							\
    assert(it);								\
    it->count = 0;							\
    a_type *cur = rbtree->rbt_root;					\
    while (cur != &rbtree->rbt_nil) {					\
	int cmp = a_cmp(RB_CMP_ARG node, cur);				\
	assert(it->count < MAX_ITER_HEIGHT);				\
	it->path[it->count++] = cur;					\
	if (cmp < 0) {							\
	    cur = rbtn_left_get(a_type, a_field, cur);			\
	} else if (cmp > 0) {						\
	    cur = rbtn_right_get(a_type, a_field, cur);			\
	} else {							\
	    return true;						\
	}								\
    }									\
    it->count = 0;							\
    return false;							\
}									\
a_attr void								\
a_prefix##ifirst(a_rbt_type *rbtree,					\
	 struct a_prefix##iterator *it)					\
{									\
    it->count = 0;							\
    if (rbtree->rbt_root != &rbtree->rbt_nil) {				\
	rbtn_iter_go_left_down(a_type, a_field, rbtree,			\
			       rbtree->rbt_root, it);			\
    }									\
}									\
a_attr void								\
a_prefix##ilast(a_rbt_type *rbtree,					\
	struct a_prefix##iterator *it)					\
{									\
    it->count = 0;							\
    if (rbtree->rbt_root != &rbtree->rbt_nil) {				\
	rbtn_iter_go_right_down(a_type, a_field, rbtree,		\
				rbtree->rbt_root, it);			\
    }									\
}									\
a_attr a_type *								\
a_prefix##inext(a_rbt_type *rbtree,					\
	struct a_prefix##iterator *it)					\
{									\
    assert(rbtree);							\
    assert(it);								\
    if (it->count <= 0) {						\
	return NULL;							\
    }									\
    a_type *ret = it->path[it->count - 1];				\
    a_type *right = rbtn_right_get(a_type, a_field,			\
				   it->path[it->count - 1]);		\
    if (right != &rbtree->rbt_nil) {					\
	rbtn_iter_go_left_down(a_type, a_field, rbtree,			\
			       right, it);				\
    } else {								\
	rbtn_iter_go_right_up(a_type, a_field, rbtree, it);		\
    }									\
    return ret;								\
}									\
a_attr a_type *								\
a_prefix##iprev(a_rbt_type *rbtree,					\
	struct a_prefix##iterator *it)					\
{									\
    assert(rbtree);							\
    assert(it);								\
    if (it->count <= 0) {						\
	return NULL;							\
    }									\
    a_type *ret = it->path[it->count - 1];				\
    a_type *left = rbtn_left_get(a_type, a_field,			\
				 it->path[it->count - 1]);		\
    if (left != &rbtree->rbt_nil) {					\
	rbtn_iter_go_right_down(a_type, a_field,			\
				rbtree,					\
				left, it);				\
    } else {								\
	rbtn_iter_go_left_up(a_type, a_field, rbtree, it);		\
    }									\
    return ret;								\
}									\
a_attr bool								\
a_prefix##isearch(a_rbt_type *rbtree, a_key key,			\
	  struct a_prefix##iterator *it)				\
{									\
    assert(rbtree);							\
    assert(it);								\
    a_type *cur = rbtree->rbt_root;					\
    it->count = 0;							\
    while (cur != &rbtree->rbt_nil) {					\
	int cmp = a_cmp_key(RB_CMP_ARG key, cur);			\
	assert(it->count < MAX_ITER_HEIGHT);				\
	it->path[it->count++] = cur;					\
	if (cmp < 0) {							\
	    cur = rbtn_left_get(a_type, a_field, cur);			\
	} else if (cmp > 0) {						\
	    cur = rbtn_right_get(a_type, a_field, cur);			\
	} else {							\
	    return true;						\
	}								\
    }									\
    it->count = 0;							\
    return false;							\
}									\
a_attr void								\
a_prefix##isearch_le(a_rbt_type *rbtree, a_key key,			\
	     struct a_prefix##iterator *it)				\
{									\
    assert(rbtree);							\
    assert(it);								\
    it->count = 0;							\
    a_type *cur = rbtree->rbt_root;					\
    int ret_count = -1;							\
    uint32_t prev_count = 0;						\
    while (cur != &rbtree->rbt_nil) {					\
	int cmp = a_cmp_key(RB_CMP_ARG key, cur);			\
	assert(it->count < MAX_ITER_HEIGHT);				\
	it->path[it->count++] = cur;					\
	if (cmp < 0) {							\
	    cur = rbtn_left_get(a_type, a_field, cur);			\
	} else if (cmp > 0) {						\
	    prev_count = it->count;					\
	    cur = rbtn_right_get(a_type, a_field, cur);			\
	} else {							\
	    ret_count = it->count;					\
	    cur = rbtn_right_get(a_type, a_field, cur);			\
	}								\
    }									\
    if (ret_count >= 0) {						\
	it->count = ret_count;						\
    } else {								\
	it->count = prev_count;						\
    }									\
}									\
a_attr void								\
a_prefix##isearch_ge(a_rbt_type *rbtree, a_key key,			\
	     struct a_prefix##iterator *it)				\
{									\
    assert(rbtree);							\
    assert(it);								\
    it->count = 0;							\
    a_type *cur = rbtree->rbt_root;					\
    int ret_count = -1;							\
    uint32_t next_count = 0;						\
    while (cur != &rbtree->rbt_nil) {					\
	int cmp = a_cmp_key(RB_CMP_ARG key, cur);			\
	assert(it->count < MAX_ITER_HEIGHT);				\
	it->path[it->count++] = cur;					\
	if (cmp < 0) {							\
	    next_count = it->count;					\
	    cur = rbtn_left_get(a_type, a_field, cur);			\
	} else if (cmp > 0) {						\
	    cur = rbtn_right_get(a_type, a_field, cur);			\
	} else {							\
	    ret_count = it->count;					\
	    cur = rbtn_left_get(a_type, a_field, cur);			\
	}								\
    }									\
    if (ret_count >= 0) {						\
	it->count = ret_count;						\
    } else {								\
	it->count = next_count;						\
    }									\
}									\
a_attr void								\
a_prefix##isearch_lt(a_rbt_type *rbtree, a_key key,			\
	     struct a_prefix##iterator *it)				\
{									\
    assert(rbtree);							\
    assert(it);								\
    it->count = 0;							\
    uint32_t prev_count = 0;						\
    a_type *cur = rbtree->rbt_root;					\
    while (cur != &rbtree->rbt_nil) {					\
	int cmp = a_cmp_key(RB_CMP_ARG key, cur);			\
	assert(it->count < MAX_ITER_HEIGHT);				\
	it->path[it->count++] = cur;					\
	if (cmp < 0) {							\
	    cur = rbtn_left_get(a_type, a_field, cur);			\
	} else if (cmp > 0) {						\
	    prev_count = it->count;					\
	    cur = rbtn_right_get(a_type, a_field, cur);			\
	} else {							\
	    cur = rbtn_left_get(a_type, a_field, cur);			\
	}								\
    }									\
    it->count = prev_count;						\
}									\
a_attr void								\
a_prefix##isearch_gt(a_rbt_type *rbtree, a_key key,			\
	     struct a_prefix##iterator *it)				\
{									\
    assert(rbtree);							\
    assert(it);								\
    it->count = 0;							\
    uint32_t next_count = 0;						\
    a_type *cur = rbtree->rbt_root;					\
    while (cur != &rbtree->rbt_nil) {					\
	int cmp = a_cmp_key(RB_CMP_ARG key, cur);			\
	assert(it->count < MAX_ITER_HEIGHT);				\
	it->path[it->count++] = cur;					\
	if (cmp < 0) {							\
	    next_count = it->count;					\
	    cur = rbtn_left_get(a_type, a_field, cur);			\
	} else if (cmp > 0) {						\
	    cur = rbtn_right_get(a_type, a_field, cur);			\
	} else {							\
	    cur = rbtn_right_get(a_type, a_field, cur);			\
	}								\
    }									\
    it->count = next_count;						\
}									\
a_attr a_type *								\
a_prefix##iter_recurse(a_rbt_type *rbtree, a_type *node,		\
  a_type *(*cb)(a_rbt_type *, a_type *, void *), void *arg) {		\
    if (node == &rbtree->rbt_nil) {					\
	return (&rbtree->rbt_nil);					\
    } else {								\
	a_type *ret = a_prefix##iter_recurse(rbtree, rbtn_left_get(	\
	  a_type, a_field, node), cb, arg);				\
	if (ret != &rbtree->rbt_nil) {					\
	    return (ret);						\
	}								\
	a_type *right = rbtn_right_get(a_type, a_field, node);		\
	ret = cb(rbtree, node, arg);					\
	if (ret != NULL) {						\
	    return (ret);						\
	}								\
	return (a_prefix##iter_recurse(rbtree, right, cb, arg));	\
    }									\
}									\
a_attr a_type *								\
a_prefix##iter_start(a_rbt_type *rbtree, a_type *start, a_type *node,	\
  a_type *(*cb)(a_rbt_type *, a_type *, void *), void *arg) {		\
    int cmp = a_cmp(RB_CMP_ARG start, node);				\
    if (cmp < 0) {							\
	a_type *ret;							\
	if ((ret = a_prefix##iter_start(rbtree, start,			\
	  rbtn_left_get(a_type, a_field, node), cb, arg)) !=		\
	  &rbtree->rbt_nil || (ret = cb(rbtree, node, arg)) != NULL) {	\
	    return (ret);						\
	}								\
	return (a_prefix##iter_recurse(rbtree, rbtn_right_get(a_type,	\
	  a_field, node), cb, arg));					\
    } else if (cmp > 0) {						\
	return (a_prefix##iter_start(rbtree, start,			\
	  rbtn_right_get(a_type, a_field, node), cb, arg));		\
    } else {								\
	a_type *ret;							\
	a_type *right = rbtn_right_get(a_type, a_field, node);		\
	if ((ret = cb(rbtree, node, arg)) != NULL) {			\
	    return (ret);						\
	}								\
	return (a_prefix##iter_recurse(rbtree, right, cb, arg));	\
    }									\
}									\
a_attr a_type *								\
a_prefix##iter(a_rbt_type *rbtree, a_type *start, a_type *(*cb)(	\
  a_rbt_type *, a_type *, void *), void *arg) {				\
    a_type *ret;							\
    if (start != NULL) {						\
	ret = a_prefix##iter_start(rbtree, start, rbtree->rbt_root,	\
	  cb, arg);							\
    } else {								\
	ret = a_prefix##iter_recurse(rbtree, rbtree->rbt_root, cb, arg);\
    }									\
    if (ret == &rbtree->rbt_nil) {					\
	ret = NULL;							\
    }									\
    return (ret);							\
}									\
a_attr a_type *								\
a_prefix##reverse_iter_recurse(a_rbt_type *rbtree, a_type *node,	\
  a_type *(*cb)(a_rbt_type *, a_type *, void *), void *arg) {		\
    if (node == &rbtree->rbt_nil) {					\
	return (&rbtree->rbt_nil);					\
    } else {								\
	a_type *ret = a_prefix##reverse_iter_recurse(rbtree,		\
	  rbtn_right_get(a_type, a_field, node), cb, arg);		\
	if (ret != &rbtree->rbt_nil) {					\
	    return (ret);						\
	}								\
	a_type *left = rbtn_left_get(a_type, a_field, node);		\
	ret = cb(rbtree, node, arg);					\
	if (ret != NULL) {						\
	    return (ret);						\
	}								\
	return (a_prefix##reverse_iter_recurse(rbtree, left, cb, arg));	\
    }									\
}									\
a_attr a_type *								\
a_prefix##reverse_iter_start(a_rbt_type *rbtree, a_type *start,		\
  a_type *node, a_type *(*cb)(a_rbt_type *, a_type *, void *),		\
  void *arg) {								\
    int cmp = a_cmp(RB_CMP_ARG start, node);				\
    if (cmp > 0) {							\
	a_type *ret;							\
	if ((ret = a_prefix##reverse_iter_start(rbtree, start,		\
	  rbtn_right_get(a_type, a_field, node), cb, arg)) !=		\
	  &rbtree->rbt_nil || (ret = cb(rbtree, node, arg)) != NULL) {	\
	    return (ret);						\
	}								\
	return (a_prefix##reverse_iter_recurse(rbtree,			\
	  rbtn_left_get(a_type, a_field, node), cb, arg));		\
    } else if (cmp < 0) {						\
	return (a_prefix##reverse_iter_start(rbtree, start,		\
	  rbtn_left_get(a_type, a_field, node), cb, arg));		\
    } else {								\
	a_type *ret;							\
	a_type *left = rbtn_left_get(a_type, a_field, node);		\
	if ((ret = cb(rbtree, node, arg)) != NULL) {			\
	    return (ret);						\
	}								\
	return (a_prefix##reverse_iter_recurse(rbtree, left, cb, arg));	\
    }									\
}									\
a_attr a_type *								\
a_prefix##reverse_iter(a_rbt_type *rbtree, a_type *start,		\
  a_type *(*cb)(a_rbt_type *, a_type *, void *), void *arg) {		\
    a_type *ret;							\
    if (start != NULL) {						\
	ret = a_prefix##reverse_iter_start(rbtree, start,		\
	  rbtree->rbt_root, cb, arg);					\
    } else {								\
	ret = a_prefix##reverse_iter_recurse(rbtree, rbtree->rbt_root,	\
	  cb, arg);							\
    }									\
    if (ret == &rbtree->rbt_nil) {					\
	ret = NULL;							\
    }									\
    return (ret);							\
}

#define	rb_gen(a_attr, a_prefix, a_rbt_type, a_type, a_field, a_cmp)	\
rb_gen_ext_key(a_attr, a_prefix, a_rbt_type, a_type, a_field, a_cmp,	\
	       a_type *, a_cmp)

#endif /* RB_H_ */
