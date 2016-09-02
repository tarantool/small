#ifndef _PM_PTHREAD_H__
#define _PM_PTHREAD_H__

#ifndef _MSC_VER

#include <pthread.h>
#include <sched.h>

#ifdef __FreeBSD__
#include <pthread_np.h>
#endif


#else // _MSC_VER=

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

struct win32_thread_t;
struct win32_thread_attr_t;

typedef struct win32_thread_t * pthread_t; 
typedef struct win32_thread_attr_t * pthread_attr_t;

/* Threads */
int pthread_create (pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int pthread_detach (pthread_t);
pthread_t pthread_self(void);
int pthread_equal (pthread_t, pthread_t);
void pthread_exit (void *);
int pthread_join (pthread_t, void **);

int pthread_attr_init (pthread_attr_t *);


#define sched_yield() Sleep(0)

#ifdef __cplusplus
}
#endif

#endif // _MSC_VER-

#endif // _PM_PTHREAD_H__