/* Poor Man MMAP (compatibility header for Windows compilers) */

#ifndef _PM_MMAP_H_
#define _PM_MMAP_H_

#ifndef _MSC_VER
# include <sys/mman.h>

#define PROT_READ_ONLY	    PROT_READ
#define PROT_READ_WRITE	    (PROT_READ | PROT_WRITE)
#define MAP_ANON_PRIVATE    (MAP_PRIVATE | MAP_ANONYMOUS)

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#else // _MSC_VER=

#include <stddef.h>
#include <sys/types.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C"
#endif

void *pm_mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset);
#define mmap pm_mmap

int pm_munmap(void *__addr, size_t __len);
#define munmap pm_munmap

int pm_mprotect(void *__addr, size_t __len, int __prot);
#define mprotect pm_mprotect


#define PROT_READ_ONLY	    PAGE_READONLY
#define PROT_READ_WRITE	    PAGE_READWRITE
#define MAP_PRIVATE	    (MEM_COMMIT | MEM_PRIVATE)
#define MAP_SHARED	    (MEM_COMMIT)
#define MAP_ANONYMOUS	    0
#define MAP_ANON_PRIVATE    (MAP_PRIVATE | MAP_ANONYMOUS)

#define MAP_FAILED	    (void*)(intptr_t)-1

#ifdef __cplusplus
}
#endif

#endif // _MSC_VER-



#endif // _PM_MMAP_H_