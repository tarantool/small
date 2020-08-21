#pragma once
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
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

#include <sys/uio.h> /* struct iovec */
#include <stdio.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <type_traits>

#include "rlist.h"

namespace tnt {

/**
 * Very basic allocator, wrapper around new/delete with fixed API.
 */
template <size_t N>
class DefaultAllocator
{
public:
	/*
 	 * TODO: avoid releasing blocks but keep them for further
 	 * allocations instead.
	 */
	static char *alloc() { return new char [N]; }
	static void free(char *ptr) { delete [] ptr; }
	/** Malloc requires no *visible* memory overhead. */
	static constexpr size_t REAL_SIZE = N;
};

/**
 * Exception safe C++ IO buffer.
 *
 * Allocator requirements (API):
 * alloc() - static allocation method, must throw an exception in case it fails.
 * Returns chunk of memory of @REAL_SIZE size (which is less or equal to N).
 * free() - static release method, takes pointer to memory allocated by @alloc
 * and frees it. Must not throw an exception.
 * REAL_SIZE - constant determines real size of allocated chunk (excluding
 * overhead taken by allocator).
 */
template <size_t N, class allocator = DefaultAllocator<N>>
class Buffer
{
private:
	/** =============== Block definition =============== */
	struct BlockBase
	{
		/** Blocks are organized into linked list. */
		struct rlist in_blocks;
	protected:
		/** Prevent base class from being instantiated. */
		BlockBase() {};
		BlockBase(BlockBase &other) { (void) other; };
		~BlockBase() {};
	};
	struct Block : BlockBase
	{
//		static constexpr size_t BLOCK_DATA_SIZE =
//			(allocator::REAL_SIZE - sizeof(BlockBase)) & ~(alignof(BlockBase) - 1);
		static constexpr size_t BLOCK_DATA_SIZE =
			allocator::REAL_SIZE - sizeof(BlockBase);
		static_assert(BLOCK_DATA_SIZE > 0,
			      "Block data size is expected to be positive value");
		static_assert(allocator::REAL_SIZE % alignof(BlockBase) == 0,
			      "Allocation size must be multiple of 16 bytes");
		/**
		 * Block itself is allocated in the same chunk so the size
		 * of available memory to keep the data is less than allocator
		 * provides.
		 */
		char data[BLOCK_DATA_SIZE];

		void* operator new(size_t size)
		{
			assert(size >= sizeof(Block));
			(void)size;
			return allocator::alloc();
		}
		void operator delete(void *ptr)
		{
			allocator::free((char *)ptr);
		}

		char  *begin() { return data; }
		char  *end()   { return data + BLOCK_DATA_SIZE; }
		Block *prev()  { return rlist_prev_entry(this, in_blocks); }
		Block *next()  { return rlist_next_entry(this, in_blocks); }
	};
	static_assert(sizeof(Block) == allocator::REAL_SIZE,
		      "size of buffer block is expected to match with "
		      "allocation size");
	static Block *newBlock(struct rlist *addToList)
	{
		Block *b = new Block;
		assert(b != nullptr);
		printf("Created block %p\n", b);
		rlist_add_tail(addToList, &b->in_blocks);
		return b;
	}
	static void delBlock(Block *b)
	{
		rlist_del(&b->in_blocks);
		printf("Deleted block %p\n", b);
		delete b;
	}
	static Block *delBlockAndPrev(Block *b)
	{
		Block *tmp = b->prev();
		delBlock(b);
		return tmp;
	}
	static Block *delBlockAndNext(Block *b)
	{
		Block *tmp = b->next();
		delBlock(b);
		return tmp;
	}

	/**
	 * Allocate a number of blocks to fit required size. This structure is
	 * used to RAII idiom to keep several blocks allocation consistent.
	 */
	struct Blocks : public rlist {
		Blocks()
		{
			rlist_create(this);
		}
		~Blocks()
		{
			while (!rlist_empty(this)) {
				Block *b = rlist_first_entry(this, Block,
							     in_blocks);
				delBlock(b);
			}
		}
	};
public:
	/** =============== Iterator definition =============== */
	class iterator : public std::iterator<std::input_iterator_tag, char>
	{
	/** Give access to private fields of iterator to Buffer. */
	friend class Buffer;
	public:
		iterator(Buffer &buffer, Block *block, char *offset, bool is_head)
			: m_buffer(buffer), m_block(block), m_position(offset)
		{
			if (is_head)
				rlist_add(&m_buffer.m_iterators, &in_iters);
			else
				rlist_add_tail(&m_buffer.m_iterators, &in_iters);
		}
		iterator(const iterator &other)
			: m_buffer(other.m_buffer),
			  m_block(other.m_block), m_position(other.m_position)
		{
			rlist_add(&other.in_iters, &in_iters);
		}
		iterator& operator = (const iterator& other)
		{
			if (this == &other)
				return *this;
			assert(&m_buffer == &other.m_buffer);
			m_block = other.m_block;
			m_position = other.m_position;
			rlist_del(&in_iters);
			rlist_add(&other.in_iters, &in_iters);
			return *this;
		}
		iterator& operator ++ ()
		{
			if (++m_position == m_block->end()) {
				m_block = rlist_next_entry(m_block, in_blocks);
				m_position = m_block->begin();
			}
			return *this;
		}
		iterator& operator += (size_t step)
		{
			while (step >= m_block->end() - m_position)
			{
				step -= m_block->end() - m_position;
				m_block = rlist_next_entry(m_block, in_blocks);
				m_position = m_block->begin();
			}
			m_position += step;
			return *this;
		}
		friend bool operator == (const iterator &a, const iterator &b)
		{
			assert(&a.m_buffer == &b.m_buffer);
			return a.m_position == b.m_position;
		}
		friend bool operator != (const iterator &a, const iterator &b)
		{
			assert(&a.m_buffer == &b.m_buffer);
			return a.m_position != b.m_position;
		}
		~iterator()
		{
			rlist_del_entry(this, in_iters);
		}
	private:
		iterator next() { return *rlist_next_entry(this, in_iters); }
		iterator prev() { return *rlist_prev_entry(this, in_iters); }
		void moveForward(size_t step)
		{
			size_t left_in_block = m_block->end() - m_position;
			while (step > left_in_block) {
				step -= left_in_block;
				m_block = m_block->next();
				left_in_block = Block::BLOCK_DATA_SIZE;
			}
			m_position = m_block->begin() + step;
		}
		void moveBackward(size_t step)
		{
			size_t left_in_block = m_position - m_block->begin();
			while (step > left_in_block) {
				step -= left_in_block;
				m_block = m_block->prev();
				left_in_block = Block::BLOCK_DATA_SIZE;
			}
			m_position = m_block->end() - step;
		}

		/** Link to the buffer iterator belongs to. */
		Buffer& m_buffer;
		/**
		 * Link in Buffer::m_iterators. Is mutable since in copy
		 * constructor/assignment operator lhs is const, but sill
		 * the link must be modified.
		 */
		mutable struct rlist in_iters;
		Block *m_block;
		/** Position inside block. */
		char *m_position;

	};
	/** =============== Buffer definition =============== */
	/** Only default constructor is available. */
	Buffer();
	Buffer(const Buffer& buf) = delete;
	Buffer& operator = (const Buffer& buf) = delete;
	~Buffer();

	/**
	 * Return iterator pointing to the start/end of buffer.
	 */
	iterator begin();
	iterator end();

	/**
	 * Copy content of @a buf (or object @a t) to the buffer's tail
	 * (append data). @a size must be less than reserved (i.e. available)
	 * free memory in buffer; UB otherwise.
	 */
	size_t addBack(const char *buf, size_t size);
	template <class T>
	size_t addBack(T&& t);

	/**
	 * Reserve memory of size @a size at the end of buffer.
	 * Return iterator pointing at the starting position of that chunk.
	 */
	iterator appendBack(size_t size);
	/**
	 * Release buffer's memory
	 */
	void dropBack(size_t size);
	void dropFront(size_t size);

	/**
	 * Insert free space of size @a size at the position @a itr pointing to.
	 * Move other iterators and reallocate space on demand. @a size must
	 * be less than block size.
	 */
	void insert(const iterator &itr, size_t size);

	/**
	 * Release memory of size @a size at the position @a itr pointing to.
	 */
	void release(const iterator &itr, size_t size);

	/** Resize memory chunk @a itr pointing to. */
	void resize(const iterator &itr, size_t old_size, size_t new_size);

	/**
	 * Copy content of @a buf of size @a size (or object @a t) to the
	 * position in buffer @a itr pointing to.
	 */
	void set(const iterator &itr, const char *buf, size_t size);
	template <class T>
	void set(const iterator &itr, T&& t);

	/**
	 * Copy content of data iterator pointing to to the buffer @a buf of
	 * size @a size.
	 */
	void get(iterator itr, char *buf, size_t size);
	template <class T>
	void get(iterator itr, T& t);

	/**
	 * Move content of buffer starting from position @a itr pointing to
	 * to array of iovecs with size of @a max_size. Each buffer block
	 * is assigned to separate iovec (so at one we copy max @a max_size
	 * blocks).
	 */
	size_t getIOV(const iterator &itr, struct iovec *vecs, size_t max_size);

	/** Return true if there's no data in the buffer. */
	bool empty() const;
private:
	Block *firstBlock();
	Block *lastBlock();

	struct rlist m_blocks;
	/** List of all data iterators created via @a begin method. */
	struct rlist m_iterators;
	/**
	 * Offset of the data in the first block. Data may start not from
	 * the beginning of the block due to ::dropFront invocation.
	 */
	char *m_begin;
	/** Last block can be partially filled, so store end border as well. */
	char *m_end;
};

template <size_t N, class allocator>
typename Buffer<N, allocator>::Block *
Buffer<N, allocator>::firstBlock()
{
	return rlist_first_entry(&m_blocks, Block, in_blocks);
}

template <size_t N, class allocator>
typename Buffer<N, allocator>::Block *
Buffer<N, allocator>::lastBlock()
{
	return rlist_last_entry(&m_blocks, Block, in_blocks);
}

template <size_t N, class allocator>
Buffer<N, allocator>::Buffer()
{
	rlist_create(&m_blocks);
	rlist_create(&m_iterators);
	m_begin = nullptr;
	m_end = nullptr;
}

template <size_t N, class allocator>
Buffer<N, allocator>::~Buffer()
{
	/* Delete blocks and release occupied memory. */
	/**
	 * TODO: that part is identical to Blocks::~Blocks().
	 * Perhaps that functionality should be moved to common parent.
	 */
	while (!rlist_empty(&m_blocks)) {
		Block *b = firstBlock();
		rlist_del(&b->in_blocks);
		delete b;
	}
}

template <size_t N, class allocator>
typename Buffer<N, allocator>::iterator
Buffer<N, allocator>::begin()
{
	return iterator(*this, firstBlock(), m_begin, true);
}

template <size_t N, class allocator>
typename Buffer<N, allocator>::iterator
Buffer<N, allocator>::end()
{
	return iterator(*this, lastBlock(), m_end, false);
}

template <size_t N, class allocator>
typename Buffer<N, allocator>::iterator
Buffer<N, allocator>::appendBack(size_t size)
{
	assert(size != 0);
	bool is_first_alloc = rlist_empty(&m_blocks);
	Block *block = is_first_alloc ? nullptr : lastBlock();
	size_t left_in_block = is_first_alloc ? 0 : block->end() - m_end;

	char *new_end = m_end;
	char *itr_offset = m_end;
	Blocks new_blocks;
	while (size > left_in_block) {
		Block *b = newBlock(&new_blocks);
		new_end = b->begin();
		size -= left_in_block;
		left_in_block = Block::BLOCK_DATA_SIZE;
	}

	if (! is_first_alloc) {
		block = firstBlock();
		itr_offset = block->begin();
	}
	rlist_splice_tail(&m_blocks, &new_blocks);

	if (is_first_alloc) {
		m_begin = firstBlock()->begin();
		itr_offset = m_begin;
		assert(itr_offset == m_begin);
		block = firstBlock();
	}
	m_end = new_end + size;

	return iterator(*this, block, itr_offset, false);
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::dropBack(size_t size)
{
	assert(size != 0);
	assert(! rlist_empty(&m_blocks));

	Block *block = lastBlock();
	size_t left_in_block = m_end - block->begin();

	/* Do not delete the block if it is empty after drop. */
	while (size > left_in_block) {
		assert(! rlist_empty(&m_blocks));
		block = delBlockAndPrev(block);
#ifndef NDEBUG
		/*
		 * Make sure there's no iterators pointing to the block
		 * to be dropped.
		 */
		if (! rlist_empty(&m_iterators)) {
			assert(rlist_last_entry(&m_iterators, iterator,
					        in_iters)->m_block != block);
		}
#endif
		m_end = block->end();
		size -= left_in_block;
		left_in_block = Block::BLOCK_DATA_SIZE;
	}
	m_end = m_end - size;
#ifndef NDEBUG
	assert(m_end >= block->begin());
	/*
	 * Two sanity checks: there's no iterators pointing to the dropped
	 * part of block; end of buffer does not cross start of buffer.
	 */
	iterator *iter = rlist_last_entry(&m_iterators, iterator, in_iters);
	if (iter->m_block == block)
		assert(iter->m_position < m_end);
	if (firstBlock() == block)
		assert(m_end >= m_begin);
#endif
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::dropFront(size_t size)
{
	assert(size != 0);
	assert(! rlist_empty(&m_blocks));

	Block *block = firstBlock();
	//printf("B size %zu end %p begin %p diff %ld\n", size, end, m_begin, end-m_begin);
	size_t left_in_block = block->end() - m_begin;

	while (size > left_in_block) {
#ifndef NDEBUG
		/*
		 * Make sure block to be dropped does not have pointing to it
		 * iterators.
		 */
		if (! rlist_empty(&m_iterators)) {
			assert(rlist_first_entry(&m_iterators, iterator,
						 in_iters)->m_block != block);
		}
#endif
		block = delBlockAndNext(block);
		m_begin = block->begin();
		size -= left_in_block;
		left_in_block = Block::BLOCK_DATA_SIZE;
	}
	m_begin += size;
	//printf("F size %zu end %p begin %p diff %ld\n",size, block->end(), m_begin, block->end()-m_begin);
#ifndef NDEBUG
	assert(m_begin <= block->end());
	iterator *iter = rlist_last_entry(&m_iterators, iterator, in_iters);
	if (iter->m_block == block)
		assert(iter->m_position >= m_begin);
	if (lastBlock() == block)
		assert(m_begin <= m_end);
#endif
}

template <size_t N, class allocator>
size_t
Buffer<N, allocator>::addBack(const char *buf, size_t size)
{
	// TODO: optimization: rewrite without iterators.
	iterator itr = appendBack(size);
	assert(itr.m_block != nullptr && itr.m_position != nullptr);
	set(itr, buf, size);
	return size;
}

template <size_t N, class allocator>
template <class T>
size_t
Buffer<N, allocator>::addBack(T&& t)
{
	static_assert(std::is_standard_layout<typename std::remove_reference<T>::type>::value,
		      "T is expected to have standard layout");
	// TODO: optimization: rewrite without iterators.
	//printf("Block sz %zu left in block %p %zu \n", Block::BLOCK_DATA_SIZE, lastBlock(), lastBlock()->end() - m_end);
	iterator itr = appendBack(sizeof(T));
	set(itr, std::forward<T>(t));
	//printf("After insert left in block %p %zu \n", lastBlock(), lastBlock()->end() - m_end);

	return sizeof(T);
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::insert(const iterator &itr, size_t size)
{
	//TODO: rewrite without iterators.
	/* Remember last block before extending the buffer. */
	Block *src_block = lastBlock();
	char *src_block_end = m_end;
	(void) appendBack(size);
	Block *dst_block = lastBlock();
	char *src = nullptr;
	char *dst = nullptr;
	/* Firstly move data in blocks. */
	size_t left_in_dst_block = m_end - dst_block->begin();
	size_t left_in_src_block = src_block_end - src_block->begin();
	if (left_in_dst_block > left_in_src_block) {
		src = src_block->begin();
		dst = dst_block->end() - left_in_src_block;
	} else {
		src = src_block_end - left_in_dst_block;
		dst = dst_block->begin();
	}
	size_t copy_chunk_sz = std::min(left_in_src_block, left_in_dst_block);
	do {
		/*
		 * During copying data in block may split into two parts
		 * which get in different blocks. So let's use two-step
		 * memcpy of data in source block.
		 */
		std::memmove(dst, src, copy_chunk_sz);
		if (left_in_dst_block > left_in_src_block) {
			left_in_dst_block -= copy_chunk_sz;
			src_block = rlist_prev_entry(src_block, in_blocks);
			src = src_block->end() - left_in_dst_block;
			left_in_src_block = Block::BLOCK_DATA_SIZE;
			dst = dst_block->begin();
			copy_chunk_sz = left_in_dst_block;
		} else {
			/* left_in_src_block >= left_in_dst_block */
			left_in_src_block -= copy_chunk_sz;
			dst_block = rlist_prev_entry(dst_block, in_blocks);
			dst = dst_block->end() - left_in_src_block;
			left_in_dst_block = Block::BLOCK_DATA_SIZE;
			src = src_block->begin();
			copy_chunk_sz = left_in_src_block;
		}
	} while (src_block != itr.m_block && src > itr.m_position);
	/* Adjust position for copy in the first block. */
	size_t diff = itr.m_position - src;
	src = itr.m_position;
	dst += diff;
	left_in_dst_block = dst_block->end() - dst;
	std::memmove(dst, src, left_in_dst_block);
	iterator cur_iter = itr;
	iterator first_iter = *rlist_first_entry(&m_iterators, iterator, in_iters);
	/* Skip all iterators with the same position. */
	do {
		cur_iter = cur_iter.next();
	} while (cur_iter.m_position == cur_iter.prev().m_position &&
		 cur_iter != first_iter);
	/* Now adjust iterators' positions. */
	while (cur_iter != first_iter) {
		cur_iter.moveForward(size);
		cur_iter = cur_iter.next();
	};
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::release(const iterator &itr, size_t size)
{
	//TODO: rewrite without iterators. Verify empty buffer case.
	Block *src_block = itr.m_block;
	Block *dst_block = itr.m_block;
	char *src = itr.m_position;
	char *dst = itr.m_position;
	/* Locate the block to start copying with. */
	size_t left_in_src_block = src_block->end() - src;
	while (size > left_in_src_block) {
		size -= left_in_src_block;
		src_block = src_block->next();
		left_in_src_block = Block::BLOCK_DATA_SIZE;
	}
	src = src_block->begin() + size;
	/* Firstly move data in blocks. */
	size_t left_in_dst_block = dst_block->end() - dst;
	left_in_src_block = src_block->end() - src;
	size_t copy_chunk_sz = std::min(left_in_src_block, left_in_dst_block);
	do {
		std::memmove(dst, src, copy_chunk_sz);
		if (left_in_dst_block > left_in_src_block) {
			left_in_dst_block -= copy_chunk_sz;
			src_block = rlist_next_entry(src_block, in_blocks);
			src = src_block->begin();
			left_in_src_block = Block::BLOCK_DATA_SIZE;
			dst += copy_chunk_sz;
			copy_chunk_sz = left_in_dst_block;
		} else {
			/* left_in_src_block >= left_in_dst_block */
			left_in_src_block -= copy_chunk_sz;
			dst_block = rlist_next_entry(dst_block, in_blocks);
			dst = dst_block->begin();
			left_in_dst_block = Block::BLOCK_DATA_SIZE;
			src += copy_chunk_sz;
			copy_chunk_sz = left_in_src_block;
		}
	} while (src_block != firstBlock());
	iterator cur_iter = itr;
	iterator first_iter = rlist_first_entry(&m_iterators, iterator, in_iters);
	/* Choose the first iterator which has the next position. */
	do {
		cur_iter = cur_iter.next();
	} while (cur_iter.m_position == cur_iter.prev().m_position &&
		 cur_iter != first_iter);
	/* Now adjust iterators' positions. */
	while (cur_iter != *rlist_first_entry(&m_iterators, iterator, in_iters)) {
		cur_iter.moveBackward(size);
		cur_iter = cur_iter.next();
	};
	/* Finally drop unused chunk. */
	dropBack(size);
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::resize(const iterator &itr, size_t size, size_t new_size)
{
	if (new_size > size)
		insert(itr, new_size - size);
	else
		release(itr, size - new_size);
}

template <size_t N, class allocator>
size_t
Buffer<N, allocator>::getIOV(const iterator &itr, struct iovec *vecs,
			     size_t max_size)
{
	assert(vecs != NULL);
	Block *block = itr.m_block;
	char *pos = itr.m_position;
	size_t vec_cnt = 0;
	for (; vec_cnt < max_size; ++vec_cnt) {
		struct iovec *vec = &vecs[vec_cnt];
		vec->iov_base = pos;
		vec->iov_len = (size_t) (block->end() - pos);
		if (block == lastBlock())
			break;
		block = rlist_next_entry(block, in_blocks);
		pos = block->begin();
	}
	return vec_cnt;
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::set(const iterator &itr, const char *buf, size_t size)
{
	Block *block = itr.m_block;
	char *pos = itr.m_position;
	size_t left_in_block = block->end() - pos;
	const char *buf_pos = buf;
	while (size > 0) {
		size_t copy_sz = std::min(size, left_in_block);
		std::memcpy(pos, buf_pos, copy_sz);
		size -= copy_sz;
		buf_pos += copy_sz;
		block = rlist_next_entry(block, in_blocks);
		pos = (char *)&block->data;
		left_in_block = Block::BLOCK_DATA_SIZE;
	}
}

template <size_t N, class allocator>
template <class T>
void
Buffer<N, allocator>::set(const iterator &itr, T&& t)
{
	/*
	 * Do not even attempt at copying non-standard classes (such as
	 * containing vtabs).
	 */
	static_assert(std::is_standard_layout<T>::value,
		      "T is expected to have standard layout");
	size_t t_size = sizeof(t);
	if (t_size <= (size_t)(itr.m_block->end() - itr.m_position))
		new(itr.m_position) T(t);
	else
		set(itr, reinterpret_cast<char *>(&t));
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::get(iterator itr, char *buf, size_t size)
{
	/*
	 * The same implementation as in ::set() method buf vice versa:
	 * buffer and data sources are swapped.
	 */
	struct Block *block = itr.m_block;
	char *pos = itr.m_position;
	size_t left_in_block = block->end() - itr.m_position;
	while (size > 0) {
		size_t copy_sz = std::min(size, left_in_block);
		std::memcpy(buf, pos, copy_sz);
		size -= copy_sz;
		buf += copy_sz;
		block = rlist_next_entry(block, in_blocks);
		pos = &block->data[0];
		left_in_block = Block::BLOCK_DATA_SIZE;
	}
}

template <size_t N, class allocator>
template <class T>
void
Buffer<N, allocator>::get(iterator itr, T& t)
{
	static_assert(std::is_standard_layout<T>::value,
		      "T is expected to have standard layout");
	size_t t_size = sizeof(t);
	if (t_size <= (size_t)(itr.m_block->end() - itr.m_position)) {

		memcpy(reinterpret_cast<T*>(&t), itr.m_position, sizeof(T));
	} else {
		get(itr, reinterpret_cast<char *>(&t), t_size);
	}
}

template <size_t N, class allocator>
bool
Buffer<N, allocator>::empty() const
{
	return m_begin == m_end;
}

} // namespace tnt {
