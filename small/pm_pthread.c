#if 0 // def _MSC_VER

#include "pm_pthread.h"
#include <malloc.h>


struct win32_thread_t {
	HANDLE threadid;
};
struct win32_thread_attr_t {
	BOOL dummy;
};

/* Threads */
pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
	       void *(*start_routine) (void *), void *arg)
{
	// FIXME - TODO
	return 1;
}

int pthread_detach(pthread_t thread)
{
	return 1;
}

pthread_t pthread_self(void)
{
	struct win32_thread_t self = { GetCurrentThread() };
	return &self;
}

int pthread_equal (pthread_t leftT, pthread_t rightT)
{
	return leftT->threadid == rightT->threadid;
}

void pthread_exit (void * value_ptr)
{
	// TODO - C++ exit thread termination and desctructors mumbo-yumbo
	ExitThread(0);
}

int pthread_join (pthread_t thread, void **return_val)
{
	return 1;
}

int pthread_attr_init(pthread_attr_t * attr)
{
	*attr = (pthread_attr_t)calloc(1, sizeof(struct win32_thread_attr_t));
	return 0;
}

#endif
