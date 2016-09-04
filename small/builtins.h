#ifndef _BUILTINS_H_
#define _BUILTINS_H_

#if defined (__GNUC__) && (__GNUC__ >= 4) || defined(__llvm__)

#define builtin_clz(v)					__builtin_clz(v)
#define builtin_clzl(v)					__builtin_clzl(v)
#define builtin_ctz(v)					__builtin_ctz(v)
#define builtin_sync_sub_and_fetch(addend,decrement)	__sync_sub_and_fetch(addend,decrement)

#elif defined(_MSC_VER)

#include <intrin.h>
#pragma intrinsic (_BitScanReverse)
#ifdef _M_X64
#pragma intrinsic (_BitScanReverse64)
#endif
#pragma intrinsic (_BitScanForward)
#pragma intrinsic (_InterlockedExchangeAdd)

unsigned long inline builtin_ctz(unsigned long  value)
{
	unsigned long trailing_zero = 0;
	_BitScanForward(&trailing_zero, value);
	return trailing_zero;
}

unsigned long inline builtin_clz(unsigned long value)
{
	unsigned long leading_zero = 0;
	_BitScanReverse(&leading_zero, value);
	return (31 - leading_zero);
}

#ifdef _M_X64
unsigned long inline builtin_clzl(unsigned __int64 value)
{
	unsigned long leading_zero = 0;
	_BitScanReverse64(&leading_zero, value);
	return (63 - leading_zero);
}
#endif

#define builtin_sync_sub_and_fetch(addend,decrement)	(_InterlockedExchangeAdd((volatile long*)(addend), -(long)(decrement)) - (decrement))

#endif

#endif // _BUILTINS_H
