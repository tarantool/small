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

/**
 * CHAR_BIT
 */
#include <limits.h>

/**
 * small_alloc uses a collection of mempools of different sizes.
 * If small_alloc stores all mempools in an array then it have to determine
 * an offset in that array where the most suitable mempool is.
 * Let's name the offset as 'size class' and the size that the corresponding
 * mempool allocates as 'class size'.
 * Historically the class sizes grow incrementally up to some point and then
 * (at some size class) grow exponentially with user-provided factor.
 * Apart from incremental part the exponential part is not very obvious.
 * Binary search and floating-point logarithm could be used for size class
 * determination but both approaches seem to be too slow.
 *
 * This module is designed for faster size class determination.
 * The idea is to use integral binary logarithm (bit search) and improve it
 * in some way in order to increase precision - allow other logarithm bases
 * along with 2.
 * Binary integral logarithm is just a position of the most significant bit of
 * a value. Let's look closer to binary representation of an allocation size
 * and size class that is calculated as binary logarithm:
 *       size      |  size class
 *    00001????..  |      x
 *    0001?????..  |    x + 1
 *    001??????..  |    x + 2
 * Let's take into account n lower bits just after the most significant
 * in the value and divide size class into 2^n subclasses. For example if n = 2:
 *       size      |  size class
 *    0000100??..  |      x
 *    0000101??..  |    x + 1
 *    0000110??..  |    x + 2
 *    0000111??..  |    x + 3
 *    000100???..  |    x + 4  <- here the size doubles, in 4 = 2^n steps.
 *    000101???..  |    x + 5
 * That gives us some kind of approximation of a logarithm with a base equal
 * to pow(2, 1 / pow(2, n)). That means that for given factor 'f' of exponent
 * we can choose such a number of bits 'n' that gives us an approximation of
 * an exponent that is close to 'f'.
 *
 * Of course if the most significant bit of a value is less than 'n' we can't
 * use the formula above. But it's not a problem since we can (and even would
 * like to!) use an incremental size class evaluation of those sizes.
 *     size      |  size class
 *    0000001    |      1  <- incremental growth.
 *    0000010    |      2
 *    0000011    |      3
 *    0000100    |      4  <- here the exponential approximation starts.
 *    0000101    |      5
 *    0000110    |      6
 *    0000111    |      7
 *    000100?    |      8
 *    000101?    |      9
 *
 * There's some implementation details. Size class is zero based, and the size
 * must be rounded up to the closest class size. Even more, we want to round
 * up size to some granularity, we doesn't want to have incremental pools of
 * sizes 1, 2, 3.., we want them to be 8, 16, 24.... All that is achieved by
 * subtracting size by one and omitting several lower bits of the size.
 */

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

struct small_class {
	/** Every class size must be a multiple of this. */
	unsigned granularity;
	/** log2(granularity), ignore those number of the lowest bit of size. */
	unsigned ignore_bits_count;
	/**
	 * A number of bits (after the most significant bit) that are used in
	 * size class evaluation ('n' in the Explanation above).
	 */
	unsigned effective_bits;
	/** 1u << effective_bits. */
	unsigned effective_size;
	/** effective_size - 1u. */
	unsigned effective_mask;
	/**
	 * By default the lowest possible allocation size (aka class size of
	 * class 0) is granularity. If a user wants different min_alloc, we
	 * simply shift sizes; min_alloc = granularity + size_shift.
	 */
	unsigned size_shift;
	/** Actually we need 'size_shift + 1', so store it. */
	unsigned size_shift_plus_1;
	/**
	 * Exponential factor, approximation of which we managed to provide.
	 * It is calculated from requested_factor, it's guaranteed that
	 * it must be in range [requested_factor/k, requested_factor*k],
	 * where k = pow(requested_factor, 0.5).
	 */
	float actual_factor;
};

/**
 * Create an instance of small_class evaluator. All args must meet the
 * requirements, undefined behaviour otherwise (at least assert).
 * @param sc - instance to create.
 * @param granularity - any class size will be a multiple of this value.
 *  Must be a power of 2 (and thus greater than zero).
 * @param desired_factor - desired factor of growth of class size.
 *  Must be in (1, 2] range. Actual factor can be different.
 * @param min_alloc - the lowest class size, must be greater than zero.
 *  The good choice is the same value as granularity.
 */
void
small_class_create(struct small_class *sc, unsigned granularity,
		   float desired_factor, unsigned min_alloc);

/**
 * Find position of the most significant bit in a value.
 * If the value is zero the behaviour is undefined.
 */
static inline unsigned
small_class_fls(unsigned value)
{
	/*
	 * Usually clz is implemented as bsr XOR 0x1f.
	 * If we add another XOR the compiler will squash 'em and leave just bsr.
	 */
	unsigned clz = __builtin_clz(value);
	return (sizeof(value) * CHAR_BIT - 1) ^ clz;
}

static inline unsigned
small_class_calc_offset_by_size(struct small_class *sc, unsigned size)
{
	/*
	 * Usually we have to decrement size in order to:
	 * 1)make zero base class.
	 * 2)round up to class size.
	 * Also here is a good place to shift size if a user wants the lowest
	 * class size to be different from granularity.
	 */
	unsigned checked_size = size - sc->size_shift_plus_1;
	/* Check overflow. */
	size = checked_size > size ? 0 : checked_size;
	/* Omit never significant bits. */
	size >>= sc->ignore_bits_count;
#ifndef SMALL_CLASS_BRANCHLESS
	if (size < sc->effective_size)
		return size; /* Linear approximation, faster part. */
	/* Get log2 base part of result. Effective bits are omitted. */
	unsigned log2 = small_class_fls(size >> sc->effective_bits);
#else
	/* Evaluation without branching */
	/*
	 * Get log2 base part of result. Effective bits are omitted.
	 * Also note that 1u is ORed to make log2 == 0 for smaller sizes.
	 */
	unsigned log2 = small_class_fls((size >> sc->effective_bits) | 1u);
#endif
	/* Effective bits (and leading 1?) in size, represent small steps. */
	unsigned linear_part = size >> log2;
	/* Log2 part, multiplied correspondingly, represent big steps. */
	unsigned log2_part = log2 << sc->effective_bits;
	/* Combine the result. */
	return linear_part + log2_part;
}

static inline unsigned
small_class_calc_size_by_offset(struct small_class *sc, unsigned cls)
{
	++cls;
	/* Effective bits (without leading 1) in size */
	unsigned linear_part = cls & sc->effective_mask;
	/* Log2 base part of the size, maybe with leading 1 of the size. */
	unsigned log2 = cls >> sc->effective_bits;
	if (log2 != 0) {
		/* Remove leading 1 from log2 part and add to linear part. */
		log2--;
		linear_part |= sc->effective_size;
	}
	/* Reassemble size and add never significant bits. */
	return sc->size_shift + (linear_part << log2 << sc->ignore_bits_count);
}

#if defined(__cplusplus)
} /* extern "C" { */
#endif /* defined(__cplusplus) */

