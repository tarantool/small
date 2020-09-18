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

#include <algorithm>
#include <cassert>
#include <cstring>
#include <type_traits>

#include "rlist.h"

namespace tnt {

/**
 * Very basic allocator, wrapper around new/delete with certain API.
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
	};
	struct Block : BlockBase
	{
		static constexpr size_t BLOCK_DATA_SIZE =
			allocator::REAL_SIZE - sizeof(BlockBase);
		/** Block itself is allocated in the same chunk. */
		char data[BLOCK_DATA_SIZE];
		static_assert(sizeof(Block) == allocator::REAL_SIZE);

		void* operator new(size_t size)
		{
			assert(size == sizeof(Block)); (void)size;
			return allocator::alloc();
		}
		void operator delete(void *ptr)
		{
			allocator::free(ptr);
		}

		char *begin() { return data; }
		char *end() { return data + BLOCK_DATA_SIZE; }
		Block *prev() { return rlist_prev_entry(this, in_blocks); }
		Block *next() { return rlist_prev_entry(this, in_blocks); }
	};
	static Block *newBlock(struct rlist *addToList)
	{
		Block *b = new Block;
		rlist_add_tail(addToList, &b->in_blocks);
		return b;
	}
	static void delBlock(Block *b)
	{
		rlist_del(&b->in_blocks);
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
		Block *tmp = b->prev();
		delBlock(b);
		return tmp;
	}

	// Allocate a number of blocks to fit required size.
	struct Blocks : rlist {
		Blocks()
		{
			rlist_create(this);
		}
		~Blocks()
		{
			while (!rlist_empty(this)) {
				Block *b = rlist_first_entry(this, Block, in_blocks);
				delBlock(b);
			}
		}
	};

	/** =============== Iterator definition =============== */
	class iterator : public std::iterator<std::input_iterator_tag, char>
	{
	public:
		iterator(Buffer &buffer, Block *block, char *offset, bool is_head)
			: m_buffer(buffer), m_block(block), m_offset(offset)
		{
			if (is_head)
				rlist_add(&m_buffer.m_iterators, &in_iters);
			else
				rlist_add_tail(&m_buffer.m_iterators, &in_iters);
		}
		iterator(iterator &other)
			: m_buffer(other.m_buffer),
			  m_block(other.m_block), m_offset(other.m_offset)
		{
			rlist_add(&other.in_iters, &in_iters);
		}
		iterator& operator = (const iterator& other)
		{
			if (this == &other)
				return *this;
			assert(&m_buffer == &other.m_buffer);
			m_block = other.m_block;
			m_offset = other.m_offset;
			rlist_del(&m_buffer);
			rlist_add(&other.in_iters, &in_iters);
			return *this;
		}
		iterator& operator ++ ()
		{
			if (++m_offset == m_block->end()) {
				m_block = rlist_next_entry(m_block, in_blocks);
				m_offset = m_block->begin();
			}
			return *this;
		}
		iterator& operator += (size_t step)
		{
			while (step >= m_block->end() - m_offset)
			{
				step -= m_block->end() - m_offset;
				m_block = rlist_next_entry(m_block, in_blocks);
				m_offset = m_block->begin();
			}
			m_offset += step;
			return *this;
		}
		friend bool operator == (const iterator &a, const iterator &b)
		{
			assert(&a.m_buffer == &b.m_buffer);
			return a.m_offset == b.m_offset;
		}
		~iterator()
		{
			rlist_del_entry(this, in_iters);
		}
	private:
		struct rlist in_iters;
		Block *m_block;
		// TODO: rename to position?
		char *m_offset;
		/** Link to the buffer iterator belongs to. */
		Buffer& m_buffer;

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
	size_t getIOV(iterator itr, struct iovec *vecs, size_t max_size);

	/** Return true if there's no data in the buffer. */
	bool empty() const;

	template <size_t alloc_sz, class alloc>
	friend void dataPosMoveForward(const Buffer &buffer,
				       CpBufferDataPosition<N, allocator> &prevDataPos,
				       CpBufferDataPosition<N, allocator> &newDataPos,
				       size_t size);
	template <size_t alloc_sz, class alloc>
	friend void dataPosMoveBack(const Buffer &buffer,
				    CpBufferDataPosition<N, allocator> &prevDataPos,
				    CpBufferDataPosition<N, allocator> &newDataPos,
				    size_t size);
private:
	char *virtualEnd();// TODO: very strange, please remove.
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
	/** Last block can be partially filled, so let's store end border. */
	char *m_end;
};


template <size_t N, class allocator>
char *
Buffer<N, allocator>::virtualEnd()
{
	Block *virtualBlock = rlist_entry(&m_blocks, Block, in_blocks);
	return virtualBlock->begin();
}

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
	Block *virtualBlock = rlist_first_entry(&m_blocks, Block, in_blocks);
	m_begin = m_end = virtualEnd();
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
		Block *b = rlist_first_entry(&m_blocks, Block, in_blocks);
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

	bool is_virtual_begin = m_begin == virtualEnd();
	bool is_virtual_end = m_end == virtualEnd();
	Block *block = lastBlock();
	size_t left_in_block = is_virtual_end ? 0 : block->end() - m_end;

	char *new_end = m_end, *itr_offset = m_end;
	Blocks new_blocks;
	while (size > left_in_block) {
		Block *b = newBlock(&new_blocks);
		new_end = b->begin();
		size -= left_in_block;
		left_in_block = Block::BLOCK_DATA_SIZE;
	}

	if (is_virtual_end) {
		block = rlist_first_entry(&new_blocks, Block, in_blocks);
		itr_offset = block->begin();
	}
	rlist_splice_tail(&m_blocks, &new_blocks);

	if (is_virtual_begin)
		m_begin = firstBlock()->begin();

	if (size == left_in_block)
		m_end = virtualEnd();
	else
		m_end = new_end + size;

	return iterator(*this, block, itr_offset, false);
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::dropBack(size_t size)
{
	assert(size != 0);
	assert(rlist_empty(&m_blocks));

	Block *block = lastBlock();
	if (m_end == virtualEnd())
		m_end = block->end();
	size_t left_in_block = m_end - block->begin();

	while (size >= left_in_block) {
		block = delBlockAndPrev(block);
		m_end = block->end();
		size -= left_in_block;
		left_in_block = Block::BLOCK_DATA_SIZE;
	}

	m_end = size == 0 ? virtualEnd() : m_end - size;
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::dropFront(size_t size)
{
	assert(size != 0);
	assert(rlist_empty(&m_blocks));

	Block *block = firstBlock();
	size_t left_in_block = block->end() - m_begin;

	while (size >= left_in_block) {
		block = delBlockAndNext(block);
		m_begin = block->begin();
		size -= left_in_block;
		left_in_block = Block::BLOCK_DATA_SIZE;
	}
	m_begin += size;
}

template <size_t N, class allocator>
size_t
Buffer<N, allocator>::addBack(const char *buf, size_t size)
{
	// TODO: optimization: rewrite without iterators.
	iterator itr = appendBack(size);
	set(itr, buf, size);
	return size;
}

template <size_t N, class allocator>
template <class T>
size_t
Buffer<N, allocator>::addBack(T&& t)
{
	static_assert(std::is_standard_layout<typename std::remove_reference<T>::type>::value);
	// TODO: optimization: rewrite without iterators.
	iterator itr = appendBack(sizeof(T));
	set(itr, std::forward<T>(t));
	return sizeof(T);
}

template <size_t N, class allocator>
struct CpBufferDataPosition<N, allocator>
dataPosMoveForward(const Buffer<N, allocator> &buffer,
		   CpBufferDataPosition<N, allocator> &prevDataPos,
		   CpBufferDataPosition<N, allocator> &newDataPos,
		   size_t size)
{
	typename Buffer<N, allocator>::Block *block = prevDataPos.block;
	char *pos = prevDataPos.offset;
	size_t blockSz = buffer.blockEnd(block) - pos;
	/* Find suitable block for iterator's new position. */
	while (size > blockSz) {
		size -= blockSz;
		block = rlist_next_entry(block, in_blocks);
		blockSz = Buffer<N, allocator>::Block::BLOCK_DATA_SIZE;
	}
	newDataPos.offset = pos + size;
	assert(newDataPos.offset <= blockSz);
	newDataPos.block = block;
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::insert(const iterator &itr, size_t size)
{
	/* Firstly move data in blocks. */
	//TODO: rewrite without iterators.
	appendBack(size);
	//...TODO: don not alloc here.

	struct Block *lastBlock =
		rlist_last_entry(&m_blocks, struct Block, in_blocks);
	size_t availableSz = blockAvailableSize(lastBlock);
	struct rlist newBlocks;
	rlist_create(&newBlocks);
	auto blocksGuard = make_scope_guard([=] {
		releaseBlocks<N, allocator>(&newBlocks);
	});
	while (availableSz < size) {
		/* Allocate enough blocs at the end of list to fit insertion. */
		struct Block *newBlock = CpBufferBlockCreate<N, allocator>();
		assert(newBlock != nullptr);
		rlist_add_entry(&newBlocks, newBlock, in_blocks);
		availableSz += Block::BLOCK_DATA_SIZE;
	}
	rlist_splice_tail(&m_blocks, &newBlocks);
	struct Block *startBlock = itr.mPosition.block;
	struct Block *srcBlock = lastBlock;
	char *src = itr.mPosition.offset;
	struct CpIterPosition dstPos;
	struct CpIterPosition newPos;
	do {
		/*
		 * During copying data in block may split into two parts
		 * which get in different blocks. So let's use two-step
		 * memcpy of data in source block.
		 */
		dstPos = { srcBlock, src };
		dataPosMoveForward(this, dstPos, newPos, size);
		struct Block *dstBlock = newPos.block;
		char *dst = newPos.offset;
		size_t dstCopySz = blockEnd(dstBlock) - dst;
		std::memmove(dst, src, dstCopySz);
		/*
		 * Copy the rest of content in source block. It may get into
		 * next to destination block.
		 */
		src += dstCopySz;
		dstPos = { srcBlock, src + dstCopySz };
		dataPosMoveForward(this, dstPos, newPos, size);
		dstBlock = newPos.block;
		dst = newPos.offset;
		dstCopySz = CpBufferBlockEnd(srcBlock) - src;
		std::memmove(dst, src, dstCopySz);
		srcBlock = rlist_prev_entry(srcBlock, in_blocks);
		src = &srcBlock->data;
	} while (srcBlock != startBlock);
	mEndOffset = (mEndOffset + size) % Block::BLOCK_DATA_SIZE;
	/* Now adjust iterators' positions. */
	iterator curIter = itr;
	while (! curIter.isLast()) {
		//TODO: check equal iterators.
		curIter = rlist_next_entry(&curIter, in_iters);
		dstPos = curIter.mPosition;
		dataPosMoveForward(this, dstPos, curIter.mPosition, size);
	};
}

template <size_t N, class allocator>
void
dataPosMoveBack(const Buffer<N, allocator> &buffer,
		CpBufferDataPosition<N, allocator> &prevDataPos,
		CpBufferDataPosition<N, allocator> &newDataPos,
		size_t size)
{
	typename Buffer<N, allocator>::Block *block = prevDataPos.block;
	char *pos = prevDataPos.offset;
	size_t blockSz = blockEnd(block) - pos;
	/* Find suitable block for iterator's new position. */
	while (blockSz < size) {
		size -= blockSz;
		block = rlist_prev_entry(block, in_blocks);
		blockSz = Buffer<N, allocator>::Block::BLOCK_DATA_SIZE;
	}
	newDataPos.offset = (char *) buffer.blockEnd(block) - size;
	assert(newDataPos.offset <= blockSz);
	newDataPos.block = block;
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::release(iterator itr, size_t size)
{
	struct Block *firstBlock =
		rlist_first_entry(&m_blocks, Block, in_blocks);
	struct Block *srcBlock = itr.mPosition.block;
	char *src = itr.mPosition.offset;
	struct CpIterPosition dstPos;
	struct CpIterPosition newPos;
	do {
		/*
		 * During copying data in block may split into two parts
		 * which get in different blocks. So let's use two-step
		 * memcpy of data in source block.
		 */
		dstPos = {srcBlock, src};
		dataPosMoveBack(this, dstPos, newPos, size);
		struct Block *dstBlock = dstPos.block;
		char *dst = newPos.offset;
		size_t dstCopySz = blockEnd(dstBlock) - dst;
		std::memmove(dst, src, dstCopySz);
		/*
		 * Copy the rest of content in source block. It may get into
		 * next to destination block.
		 */
		src += dstCopySz;
		dstPos = { srcBlock, src + dstCopySz };
		dataPosMoveBack(this, dstPos, newPos, size);
		dstBlock = newPos.block;
		dst = newPos.offset;
		dstCopySz = blockEnd(srcBlock) - src;
		std::memmove(dst, src, dstCopySz);
		srcBlock = rlist_next_entry(srcBlock, in_blocks);
		src = &srcBlock->data;
	} while (srcBlock != firstBlock);
	/* Now adjust iterators' positions. */
	iterator curIter = rlist_last_entry(&m_iterators, iterator, in_iters);
	while (curIter != itr) {
		dstPos = curIter.mPosition;
		dataPosMoveBack(this, dstPos, curIter.mPosition, size);
		curIter = rlist_next_entry(&curIter, in_iters);
	};
	/* Release all unused for now blocks. */
	struct Block *lastBlock =
		rlist_last_entry(&m_blocks, struct Block, in_blocks);
	while (size > blockUsedSize(lastBlock)) {
		struct Block *prevBlock =
			rlist_prev_entry(lastBlock, in_blocks);
		rlist_del_entry(lastBlock, in_blocks);
		size -= blockUsedSize(lastBlock);
		allocator::free(lastBlock);
		lastBlock = prevBlock;
	}
	assert(mEndOffset >= size);
	mEndOffset -= size;
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
Buffer<N, allocator>::getIOV(iterator itr, struct iovec *vecs,
			       size_t max_size)
{
	assert(vecs != NULL);
	Block *block = itr.mPosition.block;
	char *pos = itr.mPosition.offset;
	char *posEnd = blockEnd(block);
	size_t vec_cnt = 0;
	for (; vec_cnt < max_size; ++vec_cnt) {
		struct iovec *vec = vecs[vec_cnt];
		vec->iov_base = pos;
		vec->iov_len = (size_t) (posEnd - pos);
		block = rlist_next_entry(block, in_blocks);
		pos = &block->data[0];
		posEnd = blockEnd(block);
		if (blockIsFirst(block))
			break;
	}
	return vec_cnt;
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::set(const iterator &itr, const char *buf, size_t size)
{
	struct CpBufferDataPosition<N, allocator> iterPos = itr.mPosition;
	struct Block *block = iterPos.block;
	char *bufPos = iterPos.offset;
	size_t blockSz = blockEnd(block) - iterPos.offset;
	const char *dataPos = buf;
	size_t copySz = size;
	while (copySz > 0) {
		size_t dataSz = std::min(copySz, blockSz);
		std::memcpy(bufPos, dataPos, dataSz);
		copySz -= dataSz;
		dataPos += dataSz;
		assert(! blockIsLast(block) || copySz == 0);
		block = rlist_next_entry(block, in_blocks);
		bufPos = &block->data[0];
		blockSz = blockEnd(block) - bufPos;
	}
}

template <size_t N, class allocator>
template <class T>
void
Buffer<N, allocator>::set(iterator itr, T&& t)
{
	/*
	 * Do not even attempt at copying non-standard classes (such as
	 * containing vtabs).
	 */
	static_assert(! std::is_standard_layout<T>::value);
	size_t t_size = sizeof(t);
	struct CpBufferDataPosition<N, allocator> iterPos = itr.mPosition;
	if (t_size <= blockAvailableSize(iterPos.block)) {
		char *pos = iterPos.offset;
		new(pos) T(t);
	} else {
		set(itr, reinterpret_cast<char *>(&t));
	}
}

template <size_t N, class allocator>
void
Buffer<N, allocator>::get(iterator itr, char *buf, size_t size)
{
	/*
	 * The same implementation as in ::set() method buf vice versa:
	 * buffer and data sources are swapped.
	 */
	struct CpBufferDataPosition<N, allocator> iterPos = itr.mPosition;
	struct Block *block = iterPos.block;
	char *bufPos = buf;
	size_t blockSz = blockEnd(block) - iterPos.offset;
	char *dataPos = iterPos.offset;
	size_t copySz = size;
	while (copySz > 0) {
		size_t dataSz = std::min(copySz, blockSz);
		std::memcpy(bufPos, dataPos, dataSz);
		copySz -= dataSz;
		bufPos += dataSz;
		assert(! blockIsLast(block) || copySz == 0);
		block = rlist_next_entry(block, in_blocks);
		dataPos = &block->data[0];
		blockSz = blockEnd(block) - bufPos;
	}
}

template <size_t N, class allocator>
template <class T>
void
Buffer<N, allocator>::get(iterator itr, T& t)
{
	static_assert(! std::is_standard_layout<T>::value);
	size_t t_size = sizeof(t);
	struct CpBufferDataPosition<N, allocator> iterPos = itr.mPosition;
	if (t_size <= blockEnd(iterPos.block) - iterPos.offset) {
		t = *reinterpret_cast<T*>(iterPos.offset);
		memcpy(&t, iterPos.offset, sizeof(T));
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
