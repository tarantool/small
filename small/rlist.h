#ifndef TARANTOOL_RLIST_H_INCLUDED
#define TARANTOOL_RLIST_H_INCLUDED
/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <stddef.h>
#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#ifndef typeof
/* TODO: 'typeof' is a GNU extension */
#define typeof __typeof__
#endif

/**
 * list entry and head structure
 */

struct rlist {
	struct rlist *prev;
	struct rlist *next;
};

/**
 * init list head (or list entry as ins't included in list)
 */

inline static void
rlist_create(struct rlist *list)
{
	list->next = list;
	list->prev = list;
}

/**
 * add item to list
 */
inline static void
rlist_add(struct rlist *head, struct rlist *item)
{
	item->prev = head;
	item->next = head->next;
	item->prev->next = item;
	item->next->prev = item;
}

/**
 * add item to list tail
 */
inline static void
rlist_add_tail(struct rlist *head, struct rlist *item)
{
	item->next = head;
	item->prev = head->prev;
	item->prev->next = item;
	item->next->prev = item;
}

/**
 * delete element
 */
inline static void
rlist_del(struct rlist *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
	rlist_create(item);
}

inline static struct rlist *
rlist_shift(struct rlist *head)
{
        struct rlist *shift = head->next;
        head->next = shift->next;
        shift->next->prev = head;
        shift->next = shift->prev = shift;
        return shift;
}

inline static struct rlist *
rlist_shift_tail(struct rlist *head)
{
        struct rlist *shift = head->prev;
        rlist_del(shift);
        return shift;
}

/**
 * return first element
 */
inline static struct rlist *
rlist_first(struct rlist *head)
{
	return head->next;
}

/**
 * return last element
 */
inline static struct rlist *
rlist_last(struct rlist *head)
{
	return head->prev;
}

/**
 * return next element by element
 */
inline static struct rlist *
rlist_next(struct rlist *item)
{
	return item->next;
}

/**
 * return previous element
 */
inline static struct rlist *
rlist_prev(struct rlist *item)
{
	return item->prev;
}

/**
 * return TRUE if list is empty
 */
inline static int
rlist_empty(struct rlist *item)
{
	return item->next == item->prev && item->next == item;
}

/**
@brief delete from one list and add as another's head
@param to the head that will precede our entry
@param item the entry to move
*/
static inline void
rlist_move(struct rlist *to, struct rlist *item)
{
	rlist_del(item);
	rlist_add(to, item);
}

/**
@brief delete from one list and add_tail as another's head
@param to the head that will precede our entry
@param item the entry to move
*/
static inline void
rlist_move_tail(struct rlist *to, struct rlist *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
	item->next = to;
	item->prev = to->prev;
	item->prev->next = item;
	item->next->prev = item;
}

static inline void
rlist_swap(struct rlist *rhs, struct rlist *lhs)
{
	struct rlist tmp = *rhs;
	*rhs = *lhs;
	*lhs = tmp;
	/* Relink the nodes. */
	if (lhs->next == rhs)              /* Take care of empty list case */
		lhs->next = lhs;
	lhs->next->prev = lhs;
	lhs->prev->next = lhs;
	if (rhs->next == lhs)              /* Take care of empty list case */
		rhs->next = rhs;
	rhs->next->prev = rhs;
	rhs->prev->next = rhs;
}

/**
 * list head initializer
 */
#define RLIST_HEAD_INITIALIZER(name) { &(name), &(name) }

/**
 * list link node
 */
#define RLIST_LINK_INITIALIZER { 0, 0 }

/**
 * allocate and init head of list
 */
#define RLIST_HEAD(name)	\
	struct rlist name = RLIST_HEAD_INITIALIZER(name)

/**
 * return entry by list item
 */
#ifdef _MSC_VER
#define rlist_entry(item, type, member)					\
	 ((type *)( (char*)(item) - (uintptr_t)(&((type *)0)->member)))
#else
#define rlist_entry(item, type, member) ({				\
	const typeof( ((type *)0)->member ) *__mptr = (item);		\
	(type *)( (char *)__mptr - ((size_t) &((type *)0)->member) ); })
#endif

/**
 * return first entry
 */
#define rlist_first_entry(head, type, member)				\
	rlist_entry(rlist_first(head), type, member)

/**
 * Remove one element from the list and return it
 * @pre the list is not empty
 */
#define rlist_shift_entry(head, type, member)				\
        rlist_entry(rlist_shift(head), type, member)			\

/**
 * Remove one element from the list tail and return it
 * @pre the list is not empty
 */
#define rlist_shift_tail_entry(head, type, member)				\
        rlist_entry(rlist_shift_tail(head), type, member)			\


/**
 * return last entry
 * @pre the list is not empty
 */
#define rlist_last_entry(head, type, member)				\
	rlist_entry(rlist_last(head), type, member)

/**
 * return next entry
 */
#define rlist_next_entry(item, member)					\
	rlist_entry(rlist_next(&(item)->member), typeof(*item), member)

/**
 * return previous entry
 */
#define rlist_prev_entry(item, member)					\
	rlist_entry(rlist_prev(&(item)->member), typeof(*item), member)

#define rlist_prev_entry_safe(item, head, member)			\
	((rlist_prev(&(item)->member) == (head)) ? NULL :               \
	 rlist_entry(rlist_prev(&(item)->member), typeof(*item), member))

/**
 * add entry to list
 */
#define rlist_add_entry(head, item, member)				\
	rlist_add((head), &(item)->member)

/**
 * add entry to list tail
 */
#define rlist_add_tail_entry(head, item, member)			\
	rlist_add_tail((head), &(item)->member)

/**
delete from one list and add as another's head
*/
#define rlist_move_entry(to, item, member) \
	rlist_move((to), &((item)->member))

/**
delete from one list and add_tail as another's head
*/
#define rlist_move_tail_entry(to, item, member) \
	rlist_move_tail((to), &((item)->member))

/**
 * delete entry from list
 */
#define rlist_del_entry(item, member)					\
	rlist_del(&((item)->member))

/**
 * foreach through list
 */
#define rlist_foreach(item, head)					\
	for (item = rlist_first(head); item != (head); item = rlist_next(item))

 /**
 * foreach through list - safer version, gracefully handles current entry removal
 */
#define	rlist_foreach_safe(head, curr, tmp)				\
	for (curr = rlist_first(head), tmp = rlist_next(curr);		\
	     curr != (head);						\
	     curr = (tmp), tmp = rlist_next(curr))

 /**
 * foreach backward through list
 */
#define rlist_foreach_reverse(item, head)				\
	for (item = rlist_last(head); item != (head); item = rlist_prev(item))

/* NB! rlist_foreach_entry* macros are impossible to implement in non GCC-way
 * which will be compatible with MSVC which has no _typeof_ support in their
 * preprocessor. Thus all the code is reimplemented to use simple rlist_foreach[_safe]
 */

#ifndef _MSC_VER

 /**
 * foreach through all list entries
 */
#define rlist_foreach_entry(item, head, member)				\
	for (item = rlist_first_entry((head), typeof(*item), member);	\
	     &item->member != (head);					\
	     item = rlist_next_entry((item), member))

/**
 * foreach backward through all list entries
 */
#define rlist_foreach_entry_reverse(item, head, member)			\
	for (item = rlist_last_entry((head), typeof(*item), member);	\
	     &item->member != (head);					\
	     item = rlist_prev_entry((item), member))

#define	rlist_foreach_entry_safe(item, head, member, tmp)		\
	for ((item) = rlist_first_entry((head), typeof(*item), member);	\
	     &item->member != (head) &&                                 \
	     ((tmp) = rlist_next_entry((item), member));                \
	     (item) = (tmp))

#endif // _MSC_VER



#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* TARANTOOL_RLIST_H_INCLUDED */
