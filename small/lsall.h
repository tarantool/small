#ifndef LSALL_H_INCLUDED
#define LSALL_H_INCLUDED

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include "rlist.h"
#include "veb12s.h"
#include "slab_arena.h"
#include "matras.h"
#include <string.h>

#define LSALL_INTERNAL static
#define LSALL_EXPORT inline

enum lsall_slab_status {
	LSALL_FILLING,
	LSALL_NORMAL,
	LSALL_DEFRAG
};

struct lsall_stats {
	size_t system;
	size_t used;
	size_t waste;
	size_t fragmented;
	size_t reserved;
	size_t count;
};

struct lsall_slab {
	lsall_slab_status status;
	struct rlist all_link;
	struct rlist frag_link;
	struct lsall_stats stats;
	uint32_t chunks[];
};

struct lsall_slab_manager {
	struct slab_arena *arena;
	struct rlist all_slabs;
};

struct lsall_log_allocator {
	struct lsall_slab *current_slab;
	char *current_pos;
	char *end_pos;
};

struct lsall_free_holder {
	struct veb12static summary;
	struct rlist lists[4096];
};

struct lsall_defrag {
	struct rlist slabs_by_frag;
	struct lsall_slab *current_slab;
	char *current_pos;
	char *end_pos;
	size_t quota;
	size_t processed;
};

struct lsall_id_holder {
	struct matras mtab;
	uint32_t free_id;
};

struct lsall_used_chunk {
	uint32_t size;
	uint32_t id;
	char object[];
};

struct lsall_free_chunk {
	uint32_t tagged_size;
	struct rlist link;
	char nothing[];
} __attribute__((packed));

struct lsall {
	uint32_t slab_size;
	uint32_t max_alloc_size;
	uintptr_t slab_addr_mask;
	struct lsall_slab_manager slab_manager;
	struct lsall_log_allocator log_allocator;
	struct lsall_free_holder free_holder;
	struct lsall_defrag defrag;
	struct lsall_id_holder id_holder;
	struct lsall_stats stats;
};

/******************** Some constants ********************/
static const uint32_t LSALL_FREE_TAG = 1u << 31;
static const uint32_t LSALL_SIZE_MASK = LSALL_FREE_TAG - 1;
static const uint32_t LSALL_MIN_CHUNK_SIZE_IN_LIST =
	sizeof(struct lsall_free_chunk);
static const uint32_t LSALL_MIN_OBJECT_SIZE =
	sizeof(struct lsall_free_chunk) - sizeof(struct lsall_used_chunk) - 3;
static const uint32_t LSALL_MIN_OBJECT_SIZE_CLASS = LSALL_MIN_OBJECT_SIZE / 4;
static const int LSALL_DEFRAG_SHIFT = 12;
static const uint32_t LSALL_DEFRAG_FACTOR = 4;

/********** Size calculation and other helper functions **********/
LSALL_INTERNAL void
lsall_analyze_size(uint32_t size, uint32_t *size_class, uint32_t *round_size)
{
	assert(size > 0);
	uint32_t k = (__builtin_clz((size - 1) | 512) ^ 0x1f) - 9;
	uint32_t d = (size - 1) >> (k + 2);
	*size_class = (k << 7) + d;
	*round_size = (d + 1) << (k + 2);
}

LSALL_INTERNAL uint32_t
lsall_round_size_by_class(uint32_t size_class)
{
	if (size_class < 128)
		return (size_class + 1) << 2;
	return ((size_class & 127) + 129) << ((size_class >> 7) + 1);
}

LSALL_INTERNAL uint32_t
lsall_reg_size_class(uint32_t reg_size)
{
	assert(reg_size % 4 == 0);
	assert(reg_size > sizeof(struct lsall_used_chunk));
	uint32_t size = reg_size - sizeof(struct lsall_used_chunk);
	uint32_t k = (__builtin_clz(size | 512) ^ 0x1f) - 9;
	uint32_t d = size >> (k + 2);
	return (k << 7) + d - 1;
}

LSALL_INTERNAL struct lsall_used_chunk *
lsall_used_chunk_by_object(char *object)
{
	return (struct lsall_used_chunk *)
		((char *)object - offsetof(struct lsall_used_chunk, object));
}

LSALL_INTERNAL struct lsall_slab *
lsall_slab_by_object(struct lsall *a, void *object)
{
	return (struct lsall_slab *)((uintptr_t)object & a->slab_addr_mask);
}

LSALL_INTERNAL struct lsall_used_chunk *
lsall_init_used_chunk(char *reg, uint32_t size)
{
	struct lsall_used_chunk *chunk = (struct lsall_used_chunk *)reg;
	chunk->size = size;
	return chunk;
}

/******************** Statistics ********************/
LSALL_INTERNAL void
lsall_stats_init(struct lsall_stats *s)
{
	s->system = 0;
	s->used = 0;
	s->waste = 0;
	s->fragmented = 0;
	s->reserved = 0;
	s->count = 0;
}

LSALL_INTERNAL void
lsall_stats_new_from_reserved(struct lsall_stats *s,
			      size_t size, size_t round_size)
{
	s->system += sizeof(lsall_used_chunk);
	s->used += size;
	s->waste += round_size - size;
	s->reserved -= round_size + sizeof(lsall_used_chunk);
	s->count++;
}

LSALL_INTERNAL void
lsall_stats_new_from_free(struct lsall_stats *s,
			  size_t size, size_t round_size)
{
	s->system += sizeof(lsall_used_chunk);
	s->used += size;
	s->waste += round_size - size;
	s->fragmented -= round_size + sizeof(lsall_used_chunk);
	s->count++;
}

LSALL_INTERNAL void
lsall_stats_free_block(struct lsall_stats *s,
		       size_t size, size_t round_size)
{
	s->system -= sizeof(lsall_used_chunk);
	s->used -= size;
	s->waste -= round_size - size;
	s->fragmented += round_size + sizeof(lsall_used_chunk);
	s->count--;
}

LSALL_INTERNAL void
lsall_stats_defrag_free_block(struct lsall_stats *s, size_t size)
{
	s->fragmented -= size;
	s->reserved += size;
}

LSALL_INTERNAL void
lsall_stats_defrag_used_block(struct lsall_stats *s, size_t size,
			      size_t round_size)
{
	s->system -= sizeof(lsall_used_chunk);
	s->used -= size;
	s->waste -= round_size - size;
	s->reserved += round_size + sizeof(lsall_used_chunk);
	s->count--;
}

LSALL_INTERNAL void
lsall_stats_add(struct lsall_stats *to, const struct lsall_stats *what)
{
	to->system += what->system;
	to->used += what->used;
	to->waste += what->waste;
	to->fragmented += what->fragmented;
	to->reserved += what->reserved;
	to->count += what->count;
}

LSALL_INTERNAL bool
lsall_stats_equal(struct lsall_stats *a, struct lsall_stats *b)
{
	return a->system == b->system &&
	       a->used == b->used &&
	       a->waste == b->waste &&
	       a->fragmented == b->fragmented &&
	       a->reserved == b->reserved &&
	       a->count == b->count;
}

LSALL_INTERNAL size_t
lsall_stats_sum(struct lsall_stats *s)
{
	return s->system + s->used + s->waste + s->fragmented + s->reserved;
}

LSALL_INTERNAL void
lsall_stats_new_slab(struct lsall_stats *s, size_t slab_size)
{
	size_t sys = sizeof(lsall_slab) + sizeof(uint32_t);
	s->system += sys;
	s->reserved += slab_size - sys;
}

LSALL_INTERNAL void
lsall_stats_del_slab(struct lsall_stats *s, size_t slab_size)
{
	size_t sys = sizeof(lsall_slab) + sizeof(uint32_t);
	s->system -= sys;
	s->reserved -= slab_size - sys;
}

LSALL_INTERNAL void
lsall_stats_finalize_slab(struct lsall_stats *s, size_t free_space)
{
	s->fragmented += free_space;
	s->reserved -= free_space;
}

/******************** Defragmentator ********************/
LSALL_INTERNAL void
lsall_defrag_create(struct lsall *a)
{
	struct lsall_defrag *d = &a->defrag;
	rlist_create(&d->slabs_by_frag);
	d->current_slab = NULL;
	d->current_pos = NULL;
	d->quota = 0;
	d->processed = 0;
}

LSALL_INTERNAL void
lsall_defrag_destroy(struct lsall *a)
{
	(void)a;
}

LSALL_INTERNAL void
lsall_defrag_frag_increase(struct lsall *a, struct lsall_slab *slab)
{
	struct lsall_defrag *d = &a->defrag;
	while (slab->frag_link.prev != &d->slabs_by_frag) {
		struct lsall_slab *prev = rlist_prev_entry(slab, frag_link);
		if ((prev->stats.fragmented >> LSALL_DEFRAG_SHIFT) >=
		    (slab->stats.fragmented >> LSALL_DEFRAG_SHIFT))
			break;
		rlist_del_entry(slab, frag_link);
		rlist_add_tail_entry(&prev->frag_link, slab, frag_link);
	}
}

LSALL_INTERNAL void
lsall_defrag_frag_decrease(struct lsall *a, struct lsall_slab *slab)
{
	struct lsall_defrag *d = &a->defrag;
	while (slab->frag_link.next != &d->slabs_by_frag) {
		struct lsall_slab *next = rlist_next_entry(slab, frag_link);
		if ((next->stats.fragmented >> LSALL_DEFRAG_SHIFT) <=
		    (slab->stats.fragmented >> LSALL_DEFRAG_SHIFT))
			break;
		rlist_del_entry(slab, frag_link);
		rlist_add_entry(&next->frag_link, slab, frag_link);
	}
}

LSALL_INTERNAL void
lsall_defrag_use_slab(struct lsall *a, struct lsall_slab *slab)
{
	struct lsall_defrag *d = &a->defrag;
	rlist_add_tail_entry(&d->slabs_by_frag, slab, frag_link);
	lsall_defrag_frag_increase(a, slab);
}

LSALL_INTERNAL void
lsall_free_holder_detach(struct lsall *a, uint32_t size_class,
			 struct lsall_free_chunk *chunk);

LSALL_INTERNAL struct lsall_used_chunk *
lsall_free_holder_reuse_chunk(struct lsall *a, uint32_t size,
			      uint32_t size_class, uint32_t round_size);

LSALL_INTERNAL struct lsall_used_chunk *
lsall_log_allocator_get_new_chunk(struct lsall *a,
				  uint32_t size, uint32_t round_size);

LSALL_INTERNAL char **
lsall_id_holder_get(struct lsall *a, uint32_t id);

LSALL_INTERNAL void
lsall_slab_manager_free(struct lsall *a, struct lsall_slab *slab);

LSALL_INTERNAL void
lsall_defrag_select_slab(struct lsall *a)
{
	struct lsall_defrag *d = &a->defrag;
	assert(d->current_slab == NULL);
	if (rlist_empty(&d->slabs_by_frag))
		return;
	d->current_slab = rlist_first_entry(&d->slabs_by_frag,
					    struct lsall_slab,
					    frag_link);
	rlist_del_entry(d->current_slab, frag_link);
	d->current_slab->status = LSALL_DEFRAG;
	d->current_pos = (char *)d->current_slab->chunks;
	d->end_pos = (char *) d->current_slab +
		     a->slab_size - sizeof(uint32_t);
}

LSALL_INTERNAL void
lsall_defrag_add_quota(struct lsall *a, size_t size)
{
	struct lsall_defrag *d = &a->defrag;
	d->quota += size;
	while (d->processed < d->quota) {
		if (d->current_slab == NULL) {
			lsall_defrag_select_slab(a);
			if (d->current_slab == NULL) {
				d->processed = d->quota;
				return;
			}
		}
		while (*(uint32_t *)d->current_pos & LSALL_FREE_TAG) {
			struct lsall_free_chunk *chunk =
				(struct lsall_free_chunk *)(d->current_pos);
			uint32_t size = chunk->tagged_size ^ LSALL_FREE_TAG;
			assert(size);
			assert(size % 4 == 0);
			if (size >= LSALL_MIN_CHUNK_SIZE_IN_LIST) {
				uint32_t size_class = lsall_reg_size_class(size);
				lsall_free_holder_detach(a, size_class, chunk);
			}
			lsall_stats_defrag_free_block(&a->stats, size);
			lsall_stats_defrag_free_block(&d->current_slab->stats,
						      size);
			d->current_pos += size;
			assert(d->current_pos <= d->end_pos);
			if (d->current_pos == d->end_pos) {
				lsall_slab_manager_free(a, d->current_slab);
				lsall_stats_del_slab(&a->stats, a->slab_size);
				d->current_slab = NULL;
				lsall_defrag_select_slab(a);
				if (d->current_slab == NULL) {
					d->processed = d->quota;
					return;
				}
			}
		}
		struct lsall_used_chunk *chunk =
			(struct lsall_used_chunk *)(d->current_pos);
		uint32_t size = chunk->size;
		uint32_t size_class, round_size;
		lsall_analyze_size(size, &size_class, &round_size);
		uint32_t chunk_size = round_size +
				      sizeof(struct lsall_used_chunk);
		struct lsall_used_chunk *new_chunk =
			lsall_free_holder_reuse_chunk(a, size, size_class,
						      round_size);
		if (new_chunk == NULL)
			new_chunk = lsall_log_allocator_get_new_chunk(a, size,
								      round_size);
		if (new_chunk == NULL) {
			d->processed = d->quota;
			return;
		}
		memcpy(new_chunk, chunk, chunk_size);
		char **object = lsall_id_holder_get(a, chunk->id);
		*object = new_chunk->object;

		lsall_stats_defrag_used_block(&a->stats, size, round_size);
		lsall_stats_defrag_used_block(&d->current_slab->stats, size,
					      round_size);

		d->current_pos += chunk_size;
		assert(d->current_pos <= d->end_pos);
		if (d->current_pos == d->end_pos) {
			lsall_slab_manager_free(a, d->current_slab);
			lsall_stats_del_slab(&a->stats, a->slab_size);
			d->current_slab = NULL;
			lsall_defrag_select_slab(a);
			if (d->current_slab == NULL) {
				d->processed = d->quota;
				return;
			}
		}

		d->processed += chunk_size * LSALL_DEFRAG_FACTOR;
	}
}

/******************** Free holder ********************/
LSALL_INTERNAL void
lsall_free_holder_create(struct lsall *a)
{
	struct lsall_free_holder *h = &a->free_holder;
	veb12static_init(&h->summary);
	for (int i = 0; i < 4096; i++)
		rlist_create(&h->lists[i]);
}

LSALL_INTERNAL void
lsall_free_holder_destroy(struct lsall *a)
{
	(void)a;
}

LSALL_INTERNAL void
lsall_free_holder_attach(struct lsall *a, uint32_t size_class,
			 struct lsall_free_chunk *chunk)
{
	assert(size_class >= LSALL_MIN_OBJECT_SIZE_CLASS);
	struct lsall_free_holder *h = &a->free_holder;
	veb12static_insert(&h->summary, size_class);
	rlist_add_entry(&h->lists[size_class], chunk, link);
}

LSALL_INTERNAL void
lsall_free_holder_detach(struct lsall *a, uint32_t size_class,
			 struct lsall_free_chunk *chunk)
{
	assert(size_class >= LSALL_MIN_OBJECT_SIZE_CLASS);
	struct lsall_free_holder *h = &a->free_holder;
	veb12static_set(&h->summary, size_class, !rlist_almost_empty(&chunk->link));
	rlist_del_entry(chunk, link);
}

LSALL_INTERNAL struct lsall_free_chunk *
lsall_free_holder_get_closest(struct lsall *a, uint32_t size_class)
{
	assert(size_class >= LSALL_MIN_OBJECT_SIZE_CLASS);
	struct lsall_free_holder *h = &a->free_holder;
	uint32_t found_class =
		veb12static_lower_bound(&h->summary, size_class);
	if (found_class == 4096)
		return NULL;
	struct lsall_free_chunk *chunk =
		rlist_first_entry(&h->lists[found_class],
				  struct lsall_free_chunk, link);
	lsall_free_holder_detach(a, found_class, chunk);
	return chunk;
}

LSALL_INTERNAL void
lsall_free_holder_free_region(struct lsall *a, char *reg, uint32_t reg_size)
{
	assert(reg_size > 0);
	assert(reg_size % 4 == 0);
	struct lsall_free_chunk *chunk = (struct lsall_free_chunk *)reg;
	chunk->tagged_size = LSALL_FREE_TAG | reg_size;
	if (reg_size >= LSALL_MIN_CHUNK_SIZE_IN_LIST) {
		uint32_t size_class = lsall_reg_size_class(reg_size);
		lsall_free_holder_attach(a, size_class, chunk);
	}
}

LSALL_INTERNAL void
lsall_free_holder_join_region(struct lsall *a, char *reg, uint32_t *reg_size)
{
	assert(*reg_size > 0);
	assert(*reg_size % 4 == 0);
	while ((*(uint32_t *)(reg + *reg_size)) & LSALL_FREE_TAG) {
		struct lsall_free_chunk *chunk =
			(struct lsall_free_chunk *)(reg + *reg_size);
		uint32_t next_size = chunk->tagged_size ^ LSALL_FREE_TAG;
		assert(next_size > 0);
		assert(next_size % 4 == 0);
		if (next_size >= LSALL_MIN_CHUNK_SIZE_IN_LIST) {
			uint32_t size_class = lsall_reg_size_class(next_size);
			lsall_free_holder_detach(a, size_class, chunk);
		}
		*reg_size += next_size;
	}
}

LSALL_INTERNAL struct lsall_used_chunk *
lsall_free_holder_reuse_chunk(struct lsall *a, uint32_t size,
			      uint32_t size_class, uint32_t round_size)
{
	struct lsall_free_chunk *chunk =
		lsall_free_holder_get_closest(a, size_class);
	if (chunk == NULL)
		return NULL;
	struct lsall_slab *slab = lsall_slab_by_object(a, chunk);
	char *reg = (char *)chunk;
	uint32_t reg_size = chunk->tagged_size ^ LSALL_FREE_TAG;
	uint32_t chunk_size = round_size + sizeof(struct lsall_used_chunk);
	assert(reg_size >= chunk_size);
	assert(chunk_size % 4 == 0);
	if (reg_size != chunk_size) {
		lsall_free_holder_join_region(a, reg, &reg_size);
		lsall_free_holder_free_region(a, reg + chunk_size,
					      reg_size - chunk_size);
	}
	lsall_stats_new_from_free(&a->stats, size, round_size);
	lsall_stats_new_from_free(&slab->stats, size, round_size);
	if (slab->status == LSALL_NORMAL)
		lsall_defrag_frag_decrease(a, slab);
	return lsall_init_used_chunk(reg, size);
}

LSALL_INTERNAL void
lsall_free_holder_free_chunk(struct lsall *a, struct lsall_used_chunk *chunk)
{
	uint32_t size = chunk->size;
	uint32_t size_class, round_size;
	lsall_analyze_size(size, &size_class, &round_size);
	struct lsall_slab *slab = lsall_slab_by_object(a, chunk);
	char *reg = (char *)chunk;
	uint32_t reg_size = round_size + sizeof(struct lsall_used_chunk);
	lsall_free_holder_join_region(a, reg, &reg_size);
	lsall_free_holder_free_region(a, reg, reg_size);
	lsall_stats_free_block(&a->stats, size, round_size);
	lsall_stats_free_block(&slab->stats, size, round_size);
	if (slab->status == LSALL_NORMAL)
		lsall_defrag_frag_increase(a, slab);
	lsall_defrag_add_quota(a, round_size + sizeof(struct lsall_used_chunk));
}

/******************** Slab manager ********************/
LSALL_INTERNAL void
lsall_slab_manager_create(struct lsall *a, struct slab_arena *arena)
{
	struct lsall_slab_manager *m = &a->slab_manager;
	m->arena = arena;
	rlist_create(&m->all_slabs);
}

LSALL_INTERNAL void
lsall_slab_manager_destroy(struct lsall *a)
{
	struct lsall_slab_manager *m = &a->slab_manager;
	struct lsall_slab *slab;
	rlist_foreach_entry(slab, &m->all_slabs, all_link)
		slab_unmap(m->arena, slab);
}

LSALL_INTERNAL struct lsall_slab *
lsall_slab_manager_alloc(struct lsall *a)
{
	struct lsall_slab_manager *m = &a->slab_manager;
	struct lsall_slab *slab = (struct lsall_slab *)slab_map(m->arena);
	if (slab)
		rlist_add_entry(&m->all_slabs, slab, all_link);
	return slab;
}

LSALL_INTERNAL void
lsall_slab_manager_free(struct lsall *a, struct lsall_slab *slab)
{
	struct lsall_slab_manager *m = &a->slab_manager;
	rlist_del_entry(slab, all_link);
	slab_unmap(m->arena, slab);
}

/******************** Log structured allocator ********************/
LSALL_INTERNAL void
lsall_log_allocator_create(struct lsall *a)
{
	struct lsall_log_allocator *l = &a->log_allocator;
	l->current_slab = NULL;
	l->current_pos = l->end_pos = NULL;
}

LSALL_INTERNAL void
lsall_log_allocator_destroy(struct lsall *a)
{
	(void)a;
}

LSALL_INTERNAL void
lsall_log_allocator_finalize_slab(struct lsall *a)
{
	struct lsall_log_allocator *l = &a->log_allocator;
	assert(l->current_slab);
	uint32_t reg_size = (uint32_t)(l->end_pos - l->current_pos);
	if (reg_size) {
		*(uint32_t *)l->end_pos = 0;
		lsall_free_holder_free_region(a, l->current_pos, reg_size);
	}
	lsall_stats_finalize_slab(&a->stats, reg_size);
	lsall_stats_finalize_slab(&l->current_slab->stats, reg_size);
	l->current_slab->status = LSALL_NORMAL;
	lsall_defrag_use_slab(a, l->current_slab);
	l->current_slab = NULL;
}

LSALL_INTERNAL void
lsall_log_allocator_new_slab(struct lsall *a)
{
	struct lsall_log_allocator *p = &a->log_allocator;
	assert(!p->current_slab);
	struct lsall_slab *slab = lsall_slab_manager_alloc(a);
	p->current_slab = slab;
	if (slab == NULL)
		return;
	slab->status = LSALL_FILLING;
	lsall_stats_init(&slab->stats);
	lsall_stats_new_slab(&a->stats, a->slab_size);
	lsall_stats_new_slab(&slab->stats, a->slab_size);

	p->current_pos = (char *) p->current_slab->chunks;
	p->end_pos = (char *) p->current_slab + a->slab_size - sizeof(uint32_t);
}


LSALL_INTERNAL struct lsall_used_chunk *
lsall_log_allocator_get_new_chunk(struct lsall *a,
				 uint32_t size, uint32_t round_size)
{
	struct lsall_log_allocator *p = &a->log_allocator;
	uint32_t chunk_size = round_size + sizeof(struct lsall_used_chunk);
	if (p->current_slab && p->end_pos - p->current_pos < chunk_size)
		lsall_log_allocator_finalize_slab(a);
	if (!p->current_slab) {
		lsall_log_allocator_new_slab(a);
		if (!p->current_slab)
			return NULL;
	}
	struct lsall_used_chunk *chunk =
		lsall_init_used_chunk(p->current_pos, size);
	p->current_pos += chunk_size;
	*(uint32_t *)p->current_pos = 0;
	lsall_stats_new_from_reserved(&a->stats, size, round_size);
	lsall_stats_new_from_reserved(&p->current_slab->stats, size, round_size);
	return chunk;
}

/******************** ID holder ********************/
LSALL_INTERNAL void
lsall_id_holder_create(struct lsall *a, uint32_t extent_size,
		       matras_alloc_func alloc_func, matras_free_func free_func)
{
	struct lsall_id_holder *h = &a->id_holder;
	matras_create(&h->mtab, extent_size, sizeof(uint64_t),
		      alloc_func, free_func);
	h->free_id = UINT32_MAX;
}

LSALL_INTERNAL void
lsall_id_holder_destroy(struct lsall *a)
{
	struct lsall_id_holder *h = &a->id_holder;
	matras_destroy(&h->mtab);
}

LSALL_INTERNAL char **
lsall_id_holder_new(struct lsall *a, uint32_t *id)
{
	struct lsall_id_holder *h = &a->id_holder;
	if (h->free_id == UINT32_MAX) {
		return (char **)matras_alloc(&h->mtab, id);
	} else {
		*id = h->free_id;
		char **object = (char **)matras_get(&h->mtab, h->free_id);
		h->free_id = (uint32_t)((*(uint64_t *)object) >> 32);
		return object;
	}
}

LSALL_INTERNAL char **
lsall_id_holder_get(struct lsall *a, uint32_t id)
{
	struct lsall_id_holder *h = &a->id_holder;
	char **object = (char **)matras_get(&h->mtab, id);
	assert((*(uint64_t *)object) % 4 == 0);
	return object;
}

LSALL_INTERNAL void
lsall_id_holder_del(struct lsall *a, char **object, uint32_t id)
{
	struct lsall_id_holder *h = &a->id_holder;
	*(uint64_t *)object = ((uint64_t)h->free_id << 32) | 1;
	h->free_id = id;
}

/* Main methods */
LSALL_EXPORT void
lsall_create(struct lsall *a, struct slab_arena *arena, uint32_t extent_size,
	     matras_alloc_func alloc_func, matras_free_func free_func)
{
	assert(sizeof(struct lsall_free_chunk) ==
		       sizeof(uint32_t) + sizeof(struct rlist));
	assert(arena->slab_size % 4 == 0);
	assert(arena->slab_size < UINT32_MAX / 2);

	a->slab_size = arena->slab_size;
	a->max_alloc_size = arena->slab_size - sizeof(struct lsall_slab) -
		sizeof(struct lsall_used_chunk) - sizeof(uint32_t);
	a->slab_addr_mask = ~((uintptr_t)arena->slab_size - 1);

	lsall_slab_manager_create(a, arena);
	lsall_log_allocator_create(a);
	lsall_free_holder_create(a);
	lsall_defrag_create(a);
	lsall_id_holder_create(a, extent_size, alloc_func, free_func);
	lsall_stats_init(&a->stats);
}

LSALL_EXPORT void
lsall_destroy(struct lsall *a)
{
	lsall_id_holder_destroy(a);
	lsall_defrag_destroy(a);
	lsall_free_holder_destroy(a);
	lsall_log_allocator_destroy(a);
	lsall_slab_manager_destroy(a);
}

LSALL_EXPORT uint32_t
lsall_get_size(void *ptr)
{
	char *object = (char *)ptr;
	return lsall_used_chunk_by_object(object)->size;
}

LSALL_EXPORT uint32_t
lsall_get_id(void *ptr)
{
	char *object = (char *)ptr;
	return lsall_used_chunk_by_object(object)->id;
}

LSALL_EXPORT void *
lsalloc(struct lsall *a, uint32_t size, uint32_t *id)
{
	if (size > a->max_alloc_size)
		return NULL;
	assert(size > 0);
	char **object = lsall_id_holder_new(a, id);
	if (object == NULL)
		return NULL;
	uint32_t size_class, round_size;
	lsall_analyze_size(size, &size_class, &round_size);
	assert(round_size >= LSALL_MIN_OBJECT_SIZE);
	struct lsall_used_chunk *chunk =
		lsall_free_holder_reuse_chunk(a, size, size_class, round_size);
	if (chunk == NULL)
		chunk = lsall_log_allocator_get_new_chunk(a, size, round_size);
	if (chunk == NULL) {
		lsall_id_holder_del(a, object, *id);
		return NULL;
	}
	chunk->id = *id;
	*object = chunk->object;
	return chunk->object;
}


LSALL_EXPORT void *
lsall_get(struct lsall *a, uint32_t id)
{
	return *lsall_id_holder_get(a, id);
}

LSALL_EXPORT void
lsfree(struct lsall *a, uint32_t id)
{
	char **object = lsall_id_holder_get(a, id);
	struct lsall_used_chunk *chunk = lsall_used_chunk_by_object(*object);
	assert(chunk->id == id);
	lsall_free_holder_free_chunk(a, chunk);
	lsall_id_holder_del(a, object, id);
}

LSALL_EXPORT int
lsall_selfcheck(struct lsall *a, bool deep)
{
	int res = 0;
	struct lsall_free_holder *h = &a->free_holder;
	uintptr_t slab_addr_mask = a->slab_addr_mask;
	size_t slab_size = a->slab_size;
	struct rlist *all_slabs = &a->slab_manager.all_slabs;

	for (uint32_t size_class = 0; size_class < 4096; size_class++) {
		uint32_t round_size = lsall_round_size_by_class(size_class);
		uint32_t reg_size = round_size + sizeof(struct lsall_used_chunk);
		struct rlist *head = &h->lists[size_class];
		if (veb12static_has(&h->summary, size_class)) {
			if (rlist_empty(head))
				res |= 1 << 0;
			struct lsall_free_chunk *chunk;
			rlist_foreach_entry(chunk, head, link) {
				uint32_t size = chunk->tagged_size;
				size ^= LSALL_FREE_TAG;
				if (size & LSALL_FREE_TAG)
					res |= 1 << 1;
				else if (size & 3)
					res |= 1 << 2;
				else if (size < reg_size)
					res |= 1 << 3;
				if (deep) {
					struct lsall_slab *slab =
						(struct lsall_slab *)
							(slab_addr_mask &
							 (uintptr_t) chunk);
					struct lsall_slab *find;
					bool found = false;
					rlist_foreach_entry(find,
							    all_slabs,
							    all_link) {
						if (slab == find) {
							found = true;
							break;
						}
					}
					if (!found)
						res |= 1 << 4;
				}
			}
		} else {
			if (!rlist_empty(head))
				res |= 1 << 5;
		}
	}

	struct lsall_stats test_stats;
	lsall_stats_init(&test_stats);
	int filling_slabs = 0;
	int defrag_slabs = 0;
	struct lsall_slab *slab;
	rlist_foreach_entry(slab, all_slabs, all_link) {
		if (lsall_stats_sum(&slab->stats) != slab_size)
			res |= 1 << 11;
		lsall_stats_add(&test_stats, &slab->stats);
		if (slab->stats.reserved && slab->status == LSALL_NORMAL)
			res |= 1 << 13;
		if (slab->status == LSALL_FILLING)
			filling_slabs++;
		if (slab->status == LSALL_DEFRAG)
			defrag_slabs++;

		char *end_pos = (char *)slab + slab_size - sizeof(uint32_t);
		char *pos = (char *)slab + sizeof(struct lsall_slab);
		if (slab->status == LSALL_DEFRAG) {
			if (slab != a->defrag.current_slab)
				res |= 1 << 17;
			pos = a->defrag.current_pos;
		}
		while (*(uint32_t *)pos != 0) {
			uint32_t marked_size = *(uint32_t *)pos;
			uint32_t size = marked_size & LSALL_SIZE_MASK;
			if (marked_size & LSALL_FREE_TAG) {
				if (size & 3)
					res |= 1 << 6;
				if (deep
				    && size >= LSALL_MIN_CHUNK_SIZE_IN_LIST) {
					uint32_t size_class =
						lsall_reg_size_class(size);
					struct rlist *head =
						&h->lists[size_class];
					struct lsall_free_chunk *chunk =
						(struct lsall_free_chunk *) pos;
					struct lsall_free_chunk *find;
					bool found = false;
					rlist_foreach_entry(find, head, link) {
						if (chunk == find) {
							found = true;
							break;
						}
					}
					if (!found)
						res |= 1 << 8;
				}
				pos += size;
			} else {
				uint32_t size_class = 4096, round_size = 0;
				if (size)
					lsall_analyze_size(size,
							   &size_class,
							   &round_size);
				uint32_t chunk_size = round_size
					+ sizeof(struct lsall_used_chunk);
				pos += chunk_size;
			}
			if (pos > end_pos) {
				res |= 1 << 9;
				break;
			}
		}
		if (pos != end_pos && slab != a->log_allocator.current_slab)
			res |= 1 << 10;
	}

	if (!lsall_stats_equal(&a->stats, &test_stats))
		res |= 1 << 12;
	if (filling_slabs > 1)
		res |= 1 << 14;
	if (defrag_slabs > 1)
		res |= 1 << 15;

	bool first_round = true;
	size_t prev_frag;
	rlist_foreach_entry(slab, &a->defrag.slabs_by_frag, frag_link) {
		if (first_round) {
			first_round = false;
		} else {
			if ((slab->stats.fragmented >> LSALL_DEFRAG_SHIFT) >
					(prev_frag >> LSALL_DEFRAG_SHIFT))
				res |= 1 << 16;
		}
		prev_frag = slab->stats.fragmented;
	}

	return res;
}


#endif //#ifndef LSALL_H_INCLUDED
