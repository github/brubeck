#include "brubeck.h"
#include "thread_helper.h"
#include "sput.h"

#define INCREMENTS (1024*4)
#define DELTA 0.5

struct spinlock_test {
	double value;
	pthread_spinlock_t lock;
};

static void *thread_spinlock(void *ptr)
{
	struct spinlock_test *spt = ptr;
	size_t i;

	for (i = 0; i < INCREMENTS; ++i) {
		pthread_spin_lock(&spt->lock);
		{
			volatile double v = spt->value;
			v = v + DELTA;
			spt->value = v;
		}
		pthread_spin_unlock(&spt->lock);
	}

	return NULL;
}

void test_atomic_spinlocks(void)
{
	struct spinlock_test spt;

	pthread_spin_init(&spt.lock, 0);
	spt.value = 0.0;

	spawn_threads(&thread_spinlock, &spt);
	sput_fail_unless(spt.value == (double)(INCREMENTS * MAX_THREADS * DELTA),
		"spinlock doesn't race");
}
