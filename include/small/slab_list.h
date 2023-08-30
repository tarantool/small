#pragma once
/*
 * Copyright 2010-2023, Tarantool AUTHORS, please see AUTHORS file.
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
#include "rlist.h"

struct small_stats {
	size_t used;
	size_t total;
};

static inline void
small_stats_reset(struct small_stats *stats)
{
	stats->used = stats->total = 0;
}

/**
 * A general purpose list of slabs. Is used
 * to store unused slabs of a certain order in the
 * slab cache, as well as to contain allocated
 * slabs of a specialized allocator.
 */
struct slab_list {
	struct rlist slabs;
	/** Total/used bytes in this list. */
	struct small_stats stats;
};

#define slab_list_add(list, slab, member)		\
do {							\
	rlist_add_entry(&(list)->slabs, (slab), member);\
	(list)->stats.total += (slab)->size;		\
} while (0)

#define slab_list_del(list, slab, member)		\
do {							\
	rlist_del_entry((slab), member);                \
	(list)->stats.total -= (slab)->size;		\
} while (0)

static inline void
slab_list_create(struct slab_list *list)
{
	rlist_create(&list->slabs);
	small_stats_reset(&list->stats);
}
