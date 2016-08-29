#ifndef _BUILTINS_H_
#define _BUILTINS_H_

#if __has_builtin(__builtin_clz) || LLVM_GNUC_PREREQ(4, 0, 0)

#define builtin_clz(v)	__builtin_clz(v)
#define builtin_ctz(v)	__builtin_ctz(v)

#elif defined(_MSC_VER)

#include <intrin.h>
#pragma intrinsic (_BitScanReverse)
#pragma intrinsic (_BitScanForward)

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

#endif

#endif // _BUILTINS_H
