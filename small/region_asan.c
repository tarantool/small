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
#include "region.h"

/** Allocate new memory block whether for allocation or reservation. */
static SMALL_NO_SANITIZE_ADDRESS void *
region_prepare_buf(struct region *region, size_t size, size_t alignment,
		   size_t used)
{
	struct region_allocation *alloc =
			small_asan_alloc(size, alignment,
					 sizeof(struct region_allocation));
	alloc->used = used;
	alloc->alignment = alignment;
	rlist_add_entry(&region->allocations, alloc, link);

	return small_asan_payload_from_header(alloc);
}

void *
region_aligned_reserve(struct region *region, size_t size, size_t alignment)
{
	small_asan_assert(region->reserved == 0);

	/* See limit explanation in the header. */
	if (size < SMALL_REGION_MIN_RESERVE)
		size = SMALL_REGION_MIN_RESERVE;

	void *ptr = region_prepare_buf(region, size, alignment, 0);
	region->reserved = size;
	return ptr;
}

/** Allocate memory in case of prior reservation. */
static SMALL_NO_SANITIZE_ADDRESS void *
region_aligned_alloc_reserved(struct region *region, size_t size,
			     size_t alignment)
{
	small_asan_assert(size <= region->reserved);
	struct region_allocation *alloc =
			rlist_first_entry(&region->allocations,
					  struct region_allocation, link);
	small_asan_assert(alignment == alloc->alignment);

	if (small_unlikely(region->on_alloc_cb != NULL))
		region->on_alloc_cb(region, size, region->cb_arg);

	/* Poison reserved but not allocated memory. */
	char *payload = small_asan_payload_from_header(alloc);
	ASAN_POISON_MEMORY_REGION(payload + size, region->reserved - size);

	region->used += size;
	alloc->used += size;
	region->reserved = 0;

	return payload;
}

void *
region_aligned_alloc(struct region *region, size_t size, size_t alignment)
{
	small_asan_assert(size > 0);
	if (region->reserved != 0)
		return region_aligned_alloc_reserved(region, size, alignment);

	void *ptr = region_prepare_buf(region, size, alignment, size);
	if (small_unlikely(region->on_alloc_cb != NULL))
		region->on_alloc_cb(region, size, region->cb_arg);
	region->used += size;
	return ptr;
}

SMALL_NO_SANITIZE_ADDRESS void
region_truncate(struct region *region, size_t used)
{
	small_asan_assert(used <= region->used);
	size_t cut_size = region->used - used;

	struct region_allocation *alloc, *tmp;
	rlist_foreach_entry_safe(alloc, &region->allocations, link, tmp) {
		if (cut_size == 0)
			break;
		/*
		 * This implementation does not support truncating to the
		 * middle of previously allocated block.
		 */
		small_asan_assert(alloc->used <= cut_size);
		cut_size -= alloc->used;
		rlist_del_entry(alloc, link);
		small_asan_free(alloc);
	}
	region->used = used;
	region->reserved = 0;
	if (small_unlikely(region->on_truncate_cb != NULL))
		region->on_truncate_cb(region, used, region->cb_arg);
}

SMALL_NO_SANITIZE_ADDRESS void *
region_join(struct region *region, size_t size)
{
	small_asan_assert(size <= region->used);
	small_asan_assert(region->reserved == 0);
	small_asan_assert(size > 0);
	struct region_allocation *alloc =
			rlist_first_entry(&region->allocations,
					  struct region_allocation, link);
	char *ret = region_alloc(region, size);
	size_t offset = size;
	while (offset > 0) {
		size_t copy_size = alloc->used;
		if (offset < copy_size)
			copy_size = offset;
		memcpy(ret + offset - copy_size,
		       small_asan_payload_from_header(alloc), copy_size);

		offset -= copy_size;
		alloc = rlist_next_entry(alloc, link);
	}
	return ret;
}
