#ifdef _MSC_VER

#include <stddef.h>
#include <assert.h>
#include "pm_mmap.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void * pm_mmap(void * __addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset)
{
	assert(__addr == 0);
	assert(__fd = -1);
	assert(__offset == 0);
	return VirtualAlloc(__addr, __len, __flags, __prot);
}

int pm_munmap(void * __addr, size_t __len)
{
	VirtualFree(__addr, __len, MEM_DECOMMIT);
	VirtualFree(__addr, 0, MEM_RELEASE);

	return 0;
}

int pm_mprotect(void * __addr, size_t __len, int __prot)
{
	VirtualProtect(__addr, __len, __prot, NULL);
	return 0;
}

#endif
