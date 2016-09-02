/*-
 * stdatomic.h - C-style C11 atomics implementation for Visual Studio 2015
 *
 */

/*-
 * Copyright (c) 2016 Timur Safin <timur.safin@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef VSATOMIC_H__
#define	VSATOMIC_H__


#ifdef __cplusplus

#include <atomic>

using std::memory_order;
using std::memory_order_seq_cst;

using std::atomic_uintmax_t;
using std::_Uint1_t;
using std::_Uint2_t;
using std::_Uint4_t;
using std::_Uint8_t;

#define pm_atomic_compare_exchange_strong(object, expected, desired)	\
	std::atomic_compare_exchange_strong(object, expected, desired)

typedef std::_Atomic_ushort	atomic_uint16_t;
typedef std::_Atomic_uint	atomic_uint32_t;
typedef std::_Atomic_ullong	atomic_uint64_t;

#else // !__cplusplus

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <crtdbg.h>
#include "xatomic.h"

// we are Visual Studio specific
#ifndef _MSC_VER
#error "vsatomic.h does not support your compiler"
#endif

/*
 * 7.17.1 Atomic lock-free macros.
 */
#if 0

#ifndef ATOMIC_CHAR_LOCK_FREE
#  define ATOMIC_CHAR_LOCK_FREE		_ATOMIC_CHAR_LOCK_FREE
#endif
#ifndef ATOMIC_CHAR16_T_LOCK_FREE
#  define ATOMIC_CHAR16_T_LOCK_FREE	_ATOMIC_CHAR16_T_LOCK_FREE
#endif
#ifndef ATOMIC_CHAR32_T_LOCK_FREE
#  define ATOMIC_CHAR32_T_LOCK_FREE	_ATOMIC_CHAR32_T_LOCK_FREE
#endif
#ifndef ATOMIC_WCHAR_T_LOCK_FREE
#  define ATOMIC_WCHAR_T_LOCK_FREE	_ATOMIC_WCHAR_T_LOCK_FREE
#endif
#ifndef ATOMIC_SHORT_LOCK_FREE
#  define ATOMIC_SHORT_LOCK_FREE	_ATOMIC_SHORT_LOCK_FREE
#endif
#ifndef ATOMIC_INT_LOCK_FREE
#  define ATOMIC_INT_LOCK_FREE		_ATOMIC_INT_LOCK_FREE
#endif
#ifndef ATOMIC_LONG_LOCK_FREE
#  define ATOMIC_LONG_LOCK_FREE		_ATOMIC_LONG_LOCK_FREE
#endif
#ifndef ATOMIC_LLONG_LOCK_FREE
#  define ATOMIC_LLONG_LOCK_FREE	_ATOMIC_LLONG_LOCK_FREE
#endif

#endif

 /*
  * 7.17.2 Initialization.
  */
#if 0

#ifndef ATOMIC_VAR_INIT
#define	ATOMIC_VAR_INIT(value)		{ .__val = (value) }
#endif
#define	atomic_init(obj, value)		((void)((obj)->__val = (value)))

#endif

  /*
   * 7.17.3 Order and consistency.
   *
   * The memory_order_* constants that denote the barrier behaviour of the
   * atomic operations.
   */

// Visual Studio 2015 has their own enum memory_order defined in the xatomic0.h
// we has already included it via xatomic.h include


/*
 * 7.17.4 Fences.
 */

#define pm_atomic_thread_fence(order)  _Atomic_thread_fence(order)
#define pm_atomic_signal_fence(order)  _Atomic_signal_fence(order)


/*
 * 7.17.5 Lock-free property.
 */
#if 0

#ifndef atomic_is_lock_free
#define	atomic_is_lock_free(obj) \
	((void)(obj), sizeof((obj)->__val) <= _ATOMIC_MAXBYTES_LOCK_FREE)
#endif

#endif

/*
 * 7.17.6 Atomic integer types.
 *  NB! we define type(s) only in the pure C mode, C++ has their own definition
 */
#if 0

typedef _Atomic_bool		atomic_bool;
typedef _Atomic_char		atomic_char;
typedef _Atomic_schar		atomic_schar;
typedef _Atomic_uchar		atomic_uchar;
typedef _Atomic_char16_t	atomic_char16_t;
typedef _Atomic_char32_t	atomic_char32_t;
typedef _Atomic_wchar_t		atomic_wchar_t;
typedef _Atomic_short		atomic_short;
typedef _Atomic_ushort		atomic_ushort;
typedef _Atomic_int		atomic_int;
typedef _Atomic_uint		atomic_uint;
typedef _Atomic_long		atomic_long;
typedef _Atomic_ulong		atomic_ulong;
typedef _Atomic_llong		atomic_llong;
typedef _Atomic_ullong		atomic_ullong;
typedef _Atomic_address		atomic_intptr_t;
typedef _Atomic_address		atomic_size_t;
typedef _Atomic_address		atomic_uintptr_t;
typedef _Atomic_llong		atomic_ptrdiff_t;

typedef _Atomic_schar		atomic_int_least8_t;
typedef _Atomic_uchar		atomic_uint_least8_t;
typedef _Atomic_short		atomic_int_least16_t;
typedef _Atomic_ushort		atomic_uint_least16_t;
typedef _Atomic_int		atomic_int_least32_t;
typedef _Atomic_uint		atomic_uint_least32_t;
typedef _Atomic_llong		atomic_int_least64_t;
typedef _Atomic_ullong		atomic_uint_least64_t;

typedef _Atomic_schar		atomic_int_fast8_t;
typedef _Atomic_uchar		atomic_uint_fast8_t;
typedef _Atomic_int		atomic_int_fast16_t;
typedef _Atomic_uint		atomic_uint_fast16_t;
typedef _Atomic_int		atomic_int_fast32_t;
typedef _Atomic_uint		atomic_uint_fast32_t;
typedef _Atomic_llong		atomic_int_fast64_t;
typedef _Atomic_ullong		atomic_uint_fast64_t;

typedef _Atomic_llong		atomic_intmax_t;
typedef _Atomic_ullong		atomic_uintmax_t;

#else

typedef short			atomic_short;
typedef unsigned short		atomic_ushort;
typedef int			atomic_int;
typedef unsigned int		atomic_uint;
typedef long			atomic_long;
typedef unsigned long		atomic_ulong;
typedef long long		atomic_llong;
typedef unsigned long long	atomic_ullong;

typedef atomic_ushort		atomic_uint16_t;
typedef atomic_uint		atomic_uint32_t;
typedef atomic_ullong		atomic_uint64_t;

typedef atomic_ullong		atomic_uintmax_t;

#endif

/*
 * 7.17.7 Operations on atomic types.
 */

/*
 * Compiler-specific operations.
 */

 static int inline _Atomic_compare_exchange_strong_n(
	volatile void *_Tgt, void *_Exp, 
	atomic_uintmax_t _Value, size_t _Size,
	memory_order _Order1, memory_order _Order2)
{
	switch(_Size) 
	{
	case 1:	return _Atomic_compare_exchange_strong_1((_Uint1_t*)_Tgt, (_Uint1_t*)_Exp,
							(_Uint1_t)_Value, _Order1, _Order2);
	case 2:	return _Atomic_compare_exchange_strong_2((_Uint2_t*)_Tgt, (_Uint2_t*)_Exp,
							(_Uint2_t)_Value, _Order1, _Order2);
	case 4: return _Atomic_compare_exchange_strong_4((_Uint4_t*)_Tgt, (_Uint4_t*)_Exp,
							(_Uint4_t)_Value, _Order1, _Order2);
	case 8:	return _Atomic_compare_exchange_strong_8((_Uint8_t*)_Tgt, (_Uint8_t*)_Exp,
							(_Uint8_t)_Value, _Order1, _Order2);
	default:	_INVALID_MEMORY_ORDER;
	}
	return 0;
}


#define	pm_atomic_compare_exchange_strong_explicit(object, expected,	\
    desired, success, failure)						\
	_Atomic_compare_exchange_strong_n(object, expected,		\
	    (atomic_uintmax_t)desired, sizeof(desired), success, failure)
#define	pm_atomic_compare_exchange_weak_explicit(object, expected,	\
    desired, success, failure)						\
	atomic_compare_exchange_strong_explicit(object, expected,	\
	    desired, success, failure)

static atomic_uintmax_t _Atomic_exchange_n(
	volatile void *_Tgt, atomic_uintmax_t _Value, size_t _Size, memory_order _Order)
{
	switch (_Size)
	{
	case 1:	return _Atomic_exchange_1((_Uint1_t*)_Tgt, (_Uint1_t)_Value, _Order);
	case 2:	return _Atomic_exchange_2((_Uint2_t*)_Tgt, (_Uint2_t)_Value, _Order);
	case 4:	return _Atomic_exchange_4((_Uint4_t*)_Tgt, (_Uint4_t)_Value, _Order);
	case 8:	return _Atomic_exchange_8((_Uint8_t*)_Tgt, (_Uint8_t)_Value, _Order);
	default:	_INVALID_MEMORY_ORDER;
	}
	return 0;	
}

// there is no __typeof__ extension in the VC preprocessor, so expect warnings
// about conversion of larger types (atomic_uintmap_t) to some smaller types (e.g. atomic_short, 
// or similar). Sorry, I can not shut it up at the header side - caller should do it.  
#define	pm_atomic_exchange_explicit(object, desired, order)		\
	 _Atomic_exchange_n(object, desired, sizeof(desired), order)

static atomic_uintmax_t _Atomic_fetch_add_n(
	volatile void *_Tgt, atomic_uintmax_t _Value, size_t _Size, memory_order _Order)
{
	switch (_Size)
	{
	case 1:	return _Atomic_fetch_add_1((_Uint1_t*)_Tgt, (_Uint1_t)_Value, _Order);
	case 2:	return _Atomic_fetch_add_2((_Uint2_t*)_Tgt, (_Uint2_t)_Value, _Order);
	case 4:	return _Atomic_fetch_add_4((_Uint4_t*)_Tgt, (_Uint4_t)_Value, _Order);
	case 8:	return _Atomic_fetch_add_8((_Uint8_t*)_Tgt, (_Uint8_t)_Value, _Order);
	default:	_INVALID_MEMORY_ORDER;
	}
	return 0;	
}
#define	pm_atomic_fetch_add_explicit(object, operand, order)		\
	_Atomic_fetch_add_n(object, (atomic_uintmax_t)operand, sizeof(operand), order)

static atomic_uintmax_t _Atomic_fetch_and_n(
	volatile void *_Tgt, atomic_uintmax_t _Value, size_t _Size, memory_order _Order)
{
	switch (_Size)
	{
	case 1:	return _Atomic_fetch_and_1((_Uint1_t*)_Tgt, (_Uint1_t)_Value, _Order);
	case 2:	return _Atomic_fetch_and_2((_Uint2_t*)_Tgt, (_Uint2_t)_Value, _Order);
	case 4:	return _Atomic_fetch_and_4((_Uint4_t*)_Tgt, (_Uint4_t)_Value, _Order);
	case 8:	return _Atomic_fetch_and_8((_Uint8_t*)_Tgt, (_Uint8_t)_Value, _Order);
	default:	_INVALID_MEMORY_ORDER;
	}
	return 0;	
}
#define	pm_atomic_fetch_and_explicit(object, operand, order)		\
	_Atomic_fetch_and_n(object, (atomic_uintmax_t)operand, sizeof(operand), order)

static atomic_uintmax_t _Atomic_fetch_or_n(
	volatile void *_Tgt, atomic_uintmax_t _Value, size_t _Size, memory_order _Order)
{
	switch (_Size)
	{
	case 1:	return _Atomic_fetch_or_1((_Uint1_t*)_Tgt, (_Uint1_t)_Value, _Order);
	case 2:	return _Atomic_fetch_or_2((_Uint2_t*)_Tgt, (_Uint2_t)_Value, _Order);
	case 4:	return _Atomic_fetch_or_4((_Uint4_t*)_Tgt, (_Uint4_t)_Value, _Order);
	case 8:	return _Atomic_fetch_or_8((_Uint8_t*)_Tgt, (_Uint8_t)_Value, _Order);
	default:	_INVALID_MEMORY_ORDER;
	}
	return 0;	
}
#define	pm_atomic_fetch_or_explicit(object, operand, order)		\
	_Atomic_fetch_or_n(object, (atomic_uintmax_t)operand, sizeof(operand), order)

static atomic_uintmax_t _Atomic_fetch_sub_n(
	volatile void *_Tgt, atomic_uintmax_t _Value, size_t _Size, memory_order _Order)
{
	switch (_Size)
	{
	case 1:	return _Atomic_fetch_sub_1((_Uint1_t*)_Tgt, (_Uint1_t)_Value, _Order);
	case 2:	return _Atomic_fetch_sub_2((_Uint2_t*)_Tgt, (_Uint2_t)_Value, _Order);
	case 4:	return _Atomic_fetch_sub_4((_Uint4_t*)_Tgt, (_Uint4_t)_Value, _Order);
	case 8:	return _Atomic_fetch_sub_8((_Uint8_t*)_Tgt, (_Uint8_t)_Value, _Order);
	default:	_INVALID_MEMORY_ORDER;
	}
	return 0;	
}
#define	pm_atomic_fetch_sub_explicit(object, operand, order)		\
	_Atomic_fetch_sub_n(object, (atomic_uintmax_t)operand, sizeof(operand), order)

static atomic_uintmax_t _Atomic_fetch_xor_n(
	volatile void *_Tgt, atomic_uintmax_t _Value, size_t _Size, memory_order _Order)
{
	switch (_Size)
	{
	case 1:	return _Atomic_fetch_xor_1((_Uint1_t*)_Tgt, (_Uint1_t)_Value, _Order);
	case 2:	return _Atomic_fetch_xor_2((_Uint2_t*)_Tgt, (_Uint2_t)_Value, _Order);
	case 4:	return _Atomic_fetch_xor_4((_Uint4_t*)_Tgt, (_Uint4_t)_Value, _Order);
	case 8:	return _Atomic_fetch_xor_8((_Uint8_t*)_Tgt, (_Uint8_t)_Value, _Order);
	default:	_INVALID_MEMORY_ORDER;
	}
	return 0;	
}
#define	pm_atomic_fetch_xor_explicit(object, operand, order)		\
	_Atomic_fetch_xor_n(object, (atomic_uintmax_t)operand, sizeof(operand), order)

static atomic_uintmax_t _Atomic_load_n(
	volatile void *_Tgt, size_t _Size, memory_order _Order)
{
	switch (_Size)
	{
	case 1:	return _Atomic_load_1((_Uint1_t*)_Tgt, _Order);
	case 2:	return _Atomic_load_2((_Uint2_t*)_Tgt, _Order);
	case 4:	return _Atomic_load_4((_Uint4_t*)_Tgt, _Order);
	case 8:	return _Atomic_load_8((_Uint8_t*)_Tgt, _Order);
	default:	_INVALID_MEMORY_ORDER;
	}
	return 0;	
}
#define	pm_atomic_load_explicit(object, order)				\
	_Atomic_load_n(object, sizeof(*object), order)

static void inline _Atomic_store_n(
	volatile _Uint1_t *_Tgt, _Uint1_t _Value, size_t _Size, memory_order _Order)
{
	switch (_Size)
	{
	case 1:	 _Atomic_store_1((_Uint1_t*)_Tgt, _Value, _Order); break;
	case 2:	 _Atomic_store_2((_Uint2_t*)_Tgt, _Value, _Order); break;
	case 4:	 _Atomic_store_4((_Uint4_t*)_Tgt, _Value, _Order); break;
	case 8:	 _Atomic_store_8((_Uint8_t*)_Tgt, _Value, _Order); break;
	default: _INVALID_MEMORY_ORDER;
	}
}

#define	pm_atomic_store_explicit(object, desired, order)		\
	_Atomic_store_n(object, (atomic_uintmax_t)desired, sizeof(desired), order)

/*
 * Convenience functions.
 *
 * Don't provide these in kernel space. In kernel space, we should be
 * disciplined enough to always provide explicit barriers.
 */

#ifndef _KERNEL
#define	pm_atomic_compare_exchange_strong(object, expected, desired)	\
	pm_atomic_compare_exchange_strong_explicit(object, expected,	\
	    desired, memory_order_seq_cst, memory_order_seq_cst)
#define	pm_atomic_compare_exchange_weak(object, expected, desired)	\
	pm_atomic_compare_exchange_weak_explicit(object, expected,	\
	    desired, memory_order_seq_cst, memory_order_seq_cst)
#define	pm_atomic_exchange(object, desired)				\
	pm_atomic_exchange_explicit(object, desired, memory_order_seq_cst)
#define	pm_atomic_fetch_add(object, operand)				\
	pm_atomic_fetch_add_explicit(object, operand, memory_order_seq_cst)
#define	pm_atomic_fetch_and(object, operand)				\
	pm_atomic_fetch_and_explicit(object, operand, memory_order_seq_cst)
#define	pm_atomic_fetch_or(object, operand)				\
	pm_atomic_fetch_or_explicit(object, operand, memory_order_seq_cst)
#define	pm_atomic_fetch_sub(object, operand)				\
	pm_atomic_fetch_sub_explicit(object, operand, memory_order_seq_cst)
#define	pm_atomic_fetch_xor(object, operand)				\
	pm_atomic_fetch_xor_explicit(object, operand, memory_order_seq_cst)
#define	pm_atomic_load(object)						\
	pm_atomic_load_explicit(object, memory_order_seq_cst)
#define	pm_atomic_store(object, desired)				\
	pm_atomic_store_explicit(object, desired, memory_order_seq_cst)
#endif /* !_KERNEL */

/*
 * 7.17.8 Atomic flag type and operations.
 *
 * XXX: Assume atomic_bool can be used as an atomic_flag. Is there some
 * kind of compiler built-in type we could use?
 */

#if 0

typedef _Atomic_flag_t 	atomic_flag_t;

#ifndef ATOMIC_FLAG_INIT
#define	ATOMIC_FLAG_INIT		{ 0 }
#endif

#define atomic_flag_test_and_set_explicit(object, order) 		\
	_Atomic_flag_test_and_set(object, prder)
#define atomic_flag_clear_explicit(object, order) 			\
	_Atomic_flag_clear(object, order)

#ifndef _KERNEL
#define atomic_flag_test_and_set(object)				\
	atomic_flag_test_and_set_explicit(object, memory_order_seq_cst)
#define atomic_flag_clear(object)					\
	atomic_flag_clear_explicit(object, memory_order_seq_cst)
#endif /* !_KERNEL */

#endif

#endif // __cplusplus-

#endif /* !VSATOMIC_H_ */
