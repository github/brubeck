#include <stddef.h>
#include <time.h>
#include "brubeck.h"

void brubeck_backend_register_metric(struct brubeck_backend *self, struct brubeck_metric *metric)
{
	for (;;) {
		struct brubeck_metric *next = self->queue;
		metric->next = next;

		if (__sync_bool_compare_and_swap(&self->queue, next, metric))
			break;
	}
}

static void *backend__thread(void *_ptr)
{
	struct brubeck_backend *self = (struct brubeck_backend *)_ptr;

	for (;;) {
		struct timespec now, then;

		clock_gettime(CLOCK_MONOTONIC, &then);
		then.tv_sec += self->sample_freq;

		if (!self->connect(self)) {
			struct brubeck_metric *mt;

			clock_gettime(CLOCK_REALTIME, &now);
			self->tick_time = now.tv_sec;

			for (mt = self->queue; mt; mt = mt->next) {
				if (mt->expire > BRUBECK_EXPIRE_DISABLED)
					brubeck_metric_sample(mt, self->sample, self);
			}

			if (self->flush)
				self->flush(self);
		}

		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &then, NULL);
	}
	return NULL;
}

void brubeck_backend_run_threaded(struct brubeck_backend *self)
{
	pthread_create(&self->thread, NULL, &backend__thread, self);
}

