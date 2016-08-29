/* Poor Man MMAP (compatibility header for Windows compilers) */

#ifndef _PM_MMAP_H_
#define _PM_MMAP_H_

#ifndef _MSC_VER
# include <sys/mman.h>

#define PROT_READ_WRITE	    (PROT_READ | PROT_WRITE)
#define MAP_ANON_PRIVATE    (MAP_PRIVATE | MAP_ANONYMOUS)

#else

#include <stddef.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C"
#endif

void *pm_mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd, off_t __offset);
#define mmap pm_mmap

int pm_munmap(void *__addr, size_t __len);
#define munmap pm_munmap

#define PROT_READ_WRITE	    PAGE_READWRITE
#define MAP_ANON_PRIVATE    MEM_COMMIT
#define MAP_ANONYMOUS	    0

#define MAP_FAILED	    (void*)(intptr_t)-1

#ifdef __cplusplus
}
#endif

#endif

#endif // _PM_MMAP_H_