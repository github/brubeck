#ifndef __CL_THREAD_HELPER_H__
#define __CL_THREAD_HELPER_H__

#include <pthread.h>

#define MAX_THREADS 16

static inline void
spawn_threads(void *(*thread_ptr)(void*), void *ptr)
{
	size_t i;
	pthread_t threads[MAX_THREADS];

	for (i = 0; i < MAX_THREADS; ++i)
		pthread_create(&threads[i], NULL, thread_ptr, ptr);

	for (i =0; i < MAX_THREADS; ++i)
		pthread_join(threads[i], NULL);
}

#endif
