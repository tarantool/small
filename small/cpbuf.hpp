#ifndef TARANTOOL_CPBUF_H_INCLUDED
#define TARANTOOL_CPBUF_H_INCLUDED
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

#include "rlist.h"

/** Scope guard implementation borrowed from scoped_guard.h */
template <typename Functor>
struct ScopeGuard {
	Functor f;
	bool is_active;

	explicit ScopeGuard(const Functor& fun) : f(fun), is_active(true) { }
	ScopeGuard(ScopeGuard&& guard) : f(std::move(guard.f)), is_active(guard.is_active)
	{
		guard.is_active = false;
	}
	~ScopeGuard() { if (is_active) f(); }

	ScopeGuard(const ScopeGuard&) = delete;
	ScopeGuard& operator=(const ScopeGuard&) = delete;
};

template <typename Functor>
inline ScopeGuard<Functor>
make_scope_guard(Functor guard)
{
	return ScopeGuard<Functor>(guard);
}

/**
 * Very basic allocator, wrapper around new/delete with certain API.
 */
template <size_t N>
class DefaultAllocator
{
public:
	static char *alloc() { return new char [N]; }
	static void free(char *ptr) { delete [] ptr; }
	/** Malloc requires no *visible* memory overhead. */
	static constexpr size_t REAL_SIZE = N;
};

/** Forward declaration. */
template <size_t N, class allocator>
class CpBufferDataPosition;

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
class CpBuffer
{
public:
	/** =============== Block definition =============== */
	struct CpBufferBlockBase
	{
		/** Blocks are organized into linked list. */
		struct rlist in_blocks;
	};
	struct CpBufferBlock : public CpBufferBlockBase
	{
		static constexpr size_t BLOCK_DATA_SIZE =
			allocator::REAL_SIZE - sizeof(CpBufferBlockBase);
		//static constexpr
		/** Block itself is allocated in the same chunk. */
		char data[BLOCK_DATA_SIZE];
		void* operator new(size_t size, char *where)
		{
			CpBufferBlock *newBlock = (CpBufferBlock *) where;
			memset(newBlock->data, 0, BLOCK_DATA_SIZE);
			return (void *) newBlock;
		}
	};

	/** =============== Iterator definition =============== */
	class iterator : public std::iterator<std::input_iterator_tag, char>
	{
	public:
		iterator(CpBuffer<N, allocator> *buffer,
			 const CpBufferBlock &block) :
			 mBuffer(buffer),
			 mPosition(block, block.data)
		{ rlist_add(mBuffer->m_iterators, this, in_iters); };
		iterator(CpBuffer<N, allocator> *buffer,
			 const CpBufferBlock &block, char *pos) :
			 mBuffer(buffer), mPosition(block, pos)
		{ rlist_add(mBuffer->m_iterators, this, in_iters); };
		iterator(iterator &other) : mPosition(other.mPosition)
		{
			rlist_add(&other.in_iters, this, in_iters);
		}
		iterator& operator = (const iterator& other)
		{
			if (this == &other)
				return *this;
			mPosition = other.mPosition;
			in_iters = other.in_iters;
			return *this;
		}
		iterator& operator ++ ()
		{
			if (mPosition.offset ==
			    mPosition.block.data[CpBufferBlock::BLOCK_DATA_SIZE]) {
				mPosition.block =
					rlist_next_entry(mPosition.block, in_blocks);
				mPosition.offset = mPosition.block.data[0];
			} else {
				mPosition.offset++;
			}
			return *this;
		}
		~iterator()
		{
			mPosition.block = nullptr;
			mPosition.offset = nullptr;
			mBuffer = nullptr;
			rlist_del_entry(this, in_iters);
		}
	private:
		bool isLast()
		{
			return this == rlist_last_entry(&mBuffer->m_iterators,
							iterator, in_iters);
		}
		struct rlist in_iters;
		struct {
			CpBufferBlock *block;
			char *offset;
		} mPosition;
		/** Link to the buffer iterator belongs to. */
		CpBuffer<N, allocator> *mBuffer;
	};
	/** =============== Buffer definition =============== */
	/** Only default constructor is available. */
	CpBuffer();
	CpBuffer(const CpBuffer& buf) = delete;
	CpBuffer& operator = (const CpBuffer& buf) = delete;
	~CpBuffer();

	/**
	 * Return iterator pointing to the start/end of buffer.
	 */
	iterator& begin();
	iterator& end();

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
	void insert(iterator itr, size_t size);

	/**
	 * Release memory of size @a size at the position @a itr pointing to.
	 */
	void release(iterator itr, size_t size);

	/** Resize memory chunk @a itr pointing to. */
	void resize(iterator itr, size_t old_size, size_t new_size);

	/**
	 * Copy content of @a buf of size @a size (or object @a t) to the
	 * position in buffer @a itr pointing to.
	 */
	void set(iterator itr, const char *buf, size_t size);
	template <class T>
	void set(iterator itr, T&& t);

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
	friend void dataPosMoveForward(const CpBuffer &buffer,
				       CpBufferDataPosition<N, allocator> &prevDataPos,
				       CpBufferDataPosition<N, allocator> &newDataPos,
				       size_t size);
	template <size_t alloc_sz, class alloc>
	friend void dataPosMoveBack(const CpBuffer &buffer,
				    CpBufferDataPosition<N, allocator> &prevDataPos,
				    CpBufferDataPosition<N, allocator> &newDataPos,
				    size_t size);
private:
	bool blockIsLast(CpBufferBlock *block)
	{
		return block ==
			rlist_last_entry(&m_blocks, CpBufferBlock, in_blocks);
	}
	bool blockIsFirst(CpBufferBlock *block)
	{
		return block ==
		       rlist_first_entry(&m_blocks, CpBufferBlock, in_blocks);
	}
	/** Return size of memory available in the given block. */
	size_t blockAvailableSize(CpBufferBlock *block);
	/** Return size of memory occupied in the given block.*/
	size_t blockUsedSize(CpBufferBlock *block);
	/**
	 * Return pointer to the end of block: for all blocks except for the
	 * last one it is data[BLOCK_DATA_SIZE].
	 */
	const char * blockEnd(CpBufferBlock *block);
	void dropBlock(CpBufferBlock *block);
	/**
	 * Double-linked list of blocks. Each block is allocated using
	 * @a allocator template parameter.
	 */
	struct rlist m_blocks;
	/** List of all data iterators created via @a begin method. */
	struct rlist m_iterators;
	/**
	 * Offset of the data in the first block. Data may start not from
	 * the beginning of the block due to ::dropFront invocation.
	 */
	size_t mStartOffset;
	/** Last block can be partially filled, so let's store end border. */
	size_t mEndOffset;
};

/**
 * Data's (iterator's) position consists of two components: block and offset
 * inside block.
 */
template <size_t N, class allocator>
class CpBufferDataPosition
{
public:
	CpBufferDataPosition(typename CpBuffer<N, allocator>::CpBufferBlock *block,
			     char *offset) : block(block), offset(offset) {};
	CpBufferDataPosition() = default;
	typename CpBuffer<N, allocator>::CpBufferBlock *block;
	char *offset;
};

template <size_t N, class allocator>
const char *
CpBuffer<N, allocator>::blockEnd(CpBufferBlock *block)
{
	if (blockIsLast(block))
		return block->data[mEndOffset];
	return block->data[CpBufferBlock::BLOCK_DATA_SIZE - 1];
}

template <size_t N, class allocator>
size_t
CpBuffer<N, allocator>::blockAvailableSize(CpBufferBlock *block)
{
	/*
	 * We account only last part of memory in the last block as reusable.
	 * All other blocks are considered to be filled fully.
	 */
	if (blockIsLast(block))
		return CpBufferBlock::BLOCK_DATA_SIZE - mEndOffset;
	return 0;
}

template <size_t N, class allocator>
size_t
CpBuffer<N, allocator>::blockUsedSize(CpBufferBlock *block)
{

	if (blockIsLast(block))
		return mEndOffset;
	return CpBufferBlock::BLOCK_DATA_SIZE;
}

template <size_t N, class allocator = DefaultAllocator<N>>
static typename CpBuffer<N, allocator>::CpBufferBlock *
CpBufferBlockCreate()
{
	char *mem = allocator::alloc();
	return new (mem) typename CpBuffer<N, allocator>::CpBufferBlock;
}

template <size_t N, class allocator>
CpBuffer<N, allocator>::CpBuffer() : mStartOffset(0), mEndOffset(0)
{
	rlist_create(&m_blocks);
	rlist_create(&m_iterators);
}

template <size_t N, class allocator>
static void
releaseBlocks(struct rlist *blocks)
{
	typename CpBuffer<N, allocator>::CpBufferBlock *block, *tmp;
	rlist_foreach_entry_safe(block, blocks, in_blocks, tmp) {
		rlist_del_entry(block, in_blocks);
		block->~CpBufferBlock();
		allocator::free(block);
	}
}

template <size_t N, class allocator>
CpBuffer<N, allocator>::~CpBuffer()
{
	/* Delete blocks and release occupied memory. */
	releaseBlocks<N, allocator>(&m_blocks);
	mEndOffset = 0;
	mStartOffset = 0;
}

template <size_t N, class allocator>
typename CpBuffer<N, allocator>::iterator&
CpBuffer<N, allocator>::begin()
{
	CpBufferBlock *firstBlock =
		rlist_first_entry(&m_blocks, CpBufferBlock, in_blocks);
	return iterator(firstBlock, mStartOffset);
}

template <size_t N, class allocator>
typename CpBuffer<N, allocator>::iterator&
CpBuffer<N, allocator>::end()
{
	CpBufferBlock *lastBlock =
		rlist_last_entry(&m_blocks, CpBufferBlock, in_blocks);
	return iterator(lastBlock, mEndOffset);
}

template <size_t N, class allocator>
typename CpBuffer<N, allocator>::iterator
CpBuffer<N, allocator>::appendBack(size_t size)
{
	struct rlist newBlocks;
	rlist_create(&newBlocks);
	size_t availableBlockSize = 0;
	CpBufferBlock *startBlock = nullptr;
	char *startPos = mEndOffset;
	if (! rlist_empty(&m_blocks)) {
		startBlock = rlist_last_entry(&m_blocks, CpBufferBlock,
					      in_blocks);
		availableBlockSize = blockAvailableSize(startBlock);
	}
	/*
	 * If requested chunk is larger than free space in the last block
	 * than allocate new one.
	 * CpBufferBlockCreate may throw so let's use guard to follow
	 * all-or-nothing policy.
	 */
	auto blocksGuard = make_scope_guard([=] {
		releaseBlocks<N, allocator>(&newBlocks);
	});
	while (size > availableBlockSize) {
		CpBufferBlock *newBlock = CpBufferBlockCreate<N, allocator>();
		assert(newBlock != nullptr);
		rlist_add_entry(&newBlocks, newBlock, in_blocks);
		size -= CpBufferBlock::BLOCK_DATA_SIZE;
		availableBlockSize = CpBufferBlock::BLOCK_DATA_SIZE;
		mEndOffset = 0;
	}
	/*
	 * After all blocks are successfully allocated we can transfer them
	 * to the buffer's list.
	 */
	rlist_splice_tail(&m_blocks, &newBlocks);
	mEndOffset += size;
	if (startBlock == nullptr) {
		startBlock = rlist_first_entry(&m_blocks, CpBufferBlock,
					       in_blocks);
	}
	return iterator(startBlock, startPos);
}

template <size_t N, class allocator>
void
CpBuffer<N, allocator>::dropBack(size_t size)
{
	CpBufferBlock *lastBlock =
		rlist_last_entry(&m_blocks, CpBufferBlock, in_blocks);
	size_t blockUsedSize = mEndOffset;
	while (size > blockUsedSize) {
		assert(rlist_empty(&m_blocks));
		CpBufferBlock *prevBlock =
			rlist_prev_entry(lastBlock, in_blocks);
		size -= blockUsedSize;
		blockUsedSize = CpBufferBlock::BLOCK_DATA_SIZE;
		mEndOffset = CpBufferBlock::BLOCK_DATA_SIZE;
		rlist_del_entry(lastBlock, in_blocks);
		/*
		 * TODO: avoid releasing blocks but keep them for further
		 * allocations instead.
		 */
		allocator::free(lastBlock);
		lastBlock = prevBlock;
	}
	assert(mEndOffset >= size);
	mEndOffset -= size;
}

template <size_t N, class allocator>
void
CpBuffer<N, allocator>::dropFront(size_t size)
{
	assert(rlist_empty(&m_blocks));
	CpBufferBlock *firstBlock =
		rlist_first_entry(&m_blocks, struct CpBuffer, in_blocks);
	/*
	 * Drop all blocks until truncating size is less than size of
	 * single block.
	 */
	size_t blockUsedSize = mEndOffset;
	while (size > blockUsedSize) {
		assert(rlist_empty(&m_blocks));
		CpBufferBlock *nextBlock =
			rlist_next_entry(firstBlock, in_blocks);
		size -= blockUsedSize;
		blockUsedSize = CpBufferBlock::BLOCK_DATA_SIZE;
		rlist_del_entry(firstBlock, in_blocks);
		allocator::free(firstBlock);
		firstBlock = nextBlock;
		mStartOffset = 0;
	}
	/* Data in the first block can start with offset. */
	if (size > CpBufferBlock::BLOCK_DATA_SIZE - mStartOffset) {
		size -= CpBufferBlock::BLOCK_DATA_SIZE - mStartOffset;
		rlist_del_entry(firstBlock, in_blocks);
		allocator::free(firstBlock);
	}
	mStartOffset = mStartOffset + size;
}

template <size_t N, class allocator>
size_t
CpBuffer<N, allocator>::addBack(const char *buf, size_t size)
{
	iterator iter = appendBack(size);
	set(iter, buf, size);
	return size;
}

template <size_t N, class allocator>
template <class T>
size_t
CpBuffer<N, allocator>::addBack(T&& t)
{
	static_assert(std::is_standard_layout<typename std::remove_reference<T>::type>::value);
	constexpr size_t t_size = sizeof(t);
	iterator iter = appendBack(t_size);
	/*
	 * If object doesn't fit into single block, then let's fallback to
	 * block-by-block copy.
	 */
	CpBufferBlock *lastBlock =
		rlist_last_entry(&m_blocks, CpBufferBlock, in_blocks);
	if (t_size <= blockAvailableSize(iter.mPosition.block)) {
		char *bufPos = iter.mPosition.offset;
		memcpy(bufPos, &t, t_size);
	} else {
		set(iter, reinterpret_cast<char *>(&t), t_size);
	}
	return t_size;
}

template <size_t N, class allocator>
struct CpBufferDataPosition<N, allocator>
dataPosMoveForward(const CpBuffer<N, allocator> &buffer,
		   CpBufferDataPosition<N, allocator> &prevDataPos,
		   CpBufferDataPosition<N, allocator> &newDataPos,
		   size_t size)
{
	typename CpBuffer<N, allocator>::CpBufferBlock *block = prevDataPos.block;
	char *pos = prevDataPos.offset;
	size_t blockSz = buffer.blockEnd(block) - pos;
	/* Find suitable block for iterator's new position. */
	while (size > blockSz) {
		size -= blockSz;
		block = rlist_next_entry(block, in_blocks);
		blockSz = CpBuffer<N, allocator>::CpBufferBlock::BLOCK_DATA_SIZE;
	}
	newDataPos.offset = pos + size;
	assert(newDataPos.offset <= blockSz);
	newDataPos.block = block;
}

template <size_t N, class allocator>
void
CpBuffer<N, allocator>::insert(iterator iter, size_t size)
{
	/* Firstly move data in blocks. */
	struct CpBufferBlock *lastBlock =
		rlist_last_entry(&m_blocks, struct CpBufferBlock, in_blocks);
	size_t availableSz = blockAvailableSize(lastBlock);
	struct rlist newBlocks;
	rlist_create(&newBlocks);
	auto blocksGuard = make_scope_guard([=] {
		releaseBlocks<N, allocator>(&newBlocks);
	});
	while (availableSz < size) {
		/* Allocate enough blocs at the end of list to fit insertion. */
		struct CpBufferBlock *newBlock = CpBufferBlockCreate<N, allocator>();
		assert(newBlock != nullptr);
		rlist_add_entry(&newBlocks, newBlock, in_blocks);
		availableSz += CpBufferBlock::BLOCK_DATA_SIZE;
	}
	rlist_splice_tail(&m_blocks, &newBlocks);
	struct CpBufferBlock *startBlock = iter.mPosition.block;
	struct CpBufferBlock *srcBlock = lastBlock;
	char *src = iter.mPosition.offset;
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
		struct CpBufferBlock *dstBlock = newPos.block;
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
	mEndOffset = (mEndOffset + size) % CpBufferBlock::BLOCK_DATA_SIZE;
	/* Now adjust iterators' positions. */
	iterator curIter = iter;
	while (! curIter.isLast()) {
		curIter = rlist_next_entry(&curIter, in_iters);
		dstPos = curIter.mPosition;
		dataPosMoveForward(this, dstPos, curIter.mPosition, size);
	};
}

template <size_t N, class allocator>
void
dataPosMoveBack(const CpBuffer<N, allocator> &buffer,
		CpBufferDataPosition<N, allocator> &prevDataPos,
		CpBufferDataPosition<N, allocator> &newDataPos,
		size_t size)
{
	typename CpBuffer<N, allocator>::CpBufferBlock *block = prevDataPos.block;
	char *pos = prevDataPos.offset;
	size_t blockSz = blockEnd(block) - pos;
	/* Find suitable block for iterator's new position. */
	while (blockSz < size) {
		size -= blockSz;
		block = rlist_prev_entry(block, in_blocks);
		blockSz = CpBuffer<N, allocator>::CpBufferBlock::BLOCK_DATA_SIZE;
	}
	newDataPos.offset = (char *) buffer.blockEnd(block) - size;
	assert(newDataPos.offset <= blockSz);
	newDataPos.block = block;
}

template <size_t N, class allocator>
void
CpBuffer<N, allocator>::release(iterator iter, size_t size)
{
	struct CpBufferBlock *firstBlock =
		rlist_first_entry(&m_blocks, CpBufferBlock, in_blocks);
	struct CpBufferBlock *srcBlock = iter.mPosition.block;
	char *src = iter.mPosition.offset;
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
		struct CpBufferBlock *dstBlock = dstPos.block;
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
	while (curIter != iter) {
		dstPos = curIter.mPosition;
		dataPosMoveBack(this, dstPos, curIter.mPosition, size);
		curIter = rlist_next_entry(&curIter, in_iters);
	};
	/* Release all unused for now blocks. */
	struct CpBufferBlock *lastBlock =
		rlist_last_entry(&m_blocks, struct CpBufferBlock, in_blocks);
	while (size > blockUsedSize(lastBlock)) {
		struct CpBufferBlock *prevBlock =
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
CpBuffer<N, allocator>::resize(iterator iter, size_t size, size_t new_size)
{
	if (new_size > size)
		insert(iter, new_size - size);
	release(iter, size - new_size);
}

template <size_t N, class allocator>
size_t
CpBuffer<N, allocator>::getIOV(iterator itr, struct iovec *vecs,
			       size_t max_size)
{
	assert(vecs != NULL);
	CpBufferBlock *block = itr.mPosition.block;
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
CpBuffer<N, allocator>::set(iterator itr, const char *buf, size_t size)
{
	struct CpBufferDataPosition<N, allocator> iterPos = itr.mPosition;
	struct CpBufferBlock *block = iterPos.block;
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
CpBuffer<N, allocator>::set(iterator itr, T&& t)
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
CpBuffer<N, allocator>::get(iterator itr, char *buf, size_t size)
{
	/*
	 * The same implementation as in ::set() method buf vice versa:
	 * buffer and data sources are swapped.
	 */
	struct CpBufferDataPosition<N, allocator> iterPos = itr.mPosition;
	struct CpBufferBlock *block = iterPos.block;
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
CpBuffer<N, allocator>::get(iterator itr, T& t)
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
CpBuffer<N, allocator>::empty() const
{
	return 0;
}

#endif /* TARANTOOL_CPBUF_H_INCLUDED */
