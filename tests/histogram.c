#include "brubeck.h"
#include "sput.h"
#include "thread_helper.h"

#define ITERS (4096 * 8)

struct histo_test {
	struct brubeck_histo h;
	pthread_spinlock_t lock;
};

static void *thread_histo(void *ptr)
{
	struct histo_test *t = ptr;
	size_t i;
	
	for (i = 0; i < ITERS; ++i) {
		if (rand() % 2 == 0) {
			struct brubeck_histo_sample hsample;
			pthread_spin_lock(&t->lock);
			{
				brubeck_histo_sample(&hsample, &t->h);
			}
			pthread_spin_unlock(&t->lock);
		} else {
			pthread_spin_lock(&t->lock);
			{
				brubeck_histo_push(&t->h, 0.42);
			}
			pthread_spin_unlock(&t->lock);
		}
	}

	return NULL;
}

void test_histogram__sampling(void)
{
	struct histo_test test;

	memset(&test.h, 0x0, sizeof(test.h));
	pthread_spin_init(&test.lock, 0);
	spawn_threads(&thread_histo, &test);
}

void test_histogram__single_element(void)
{
	struct brubeck_histo h;
	struct brubeck_histo_sample sample;

	memset(&h, 0x0, sizeof(h));

	brubeck_histo_push(&h, 42.0);
	sput_fail_unless(h.size == 1, "push value to histogram");

	brubeck_histo_sample(&sample, &h);

	sput_fail_unless(sample.min == 42.0, 0);
	sput_fail_unless(sample.max == 42.0, 0);
	sput_fail_unless(sample.percentile[3] == 42.0, 0);
	sput_fail_unless(sample.mean == 42.0, 0);
	sput_fail_unless(sample.count == 1, 0);
	sput_fail_unless(sample.sum == 42.0, 0);
}

void test_histogram__large_range(void)
{
	struct brubeck_histo h;
	struct brubeck_histo_sample sample;

	memset(&h, 0x0, sizeof(h));

	brubeck_histo_push(&h, 1.3e12);
	brubeck_histo_push(&h, 42.0);
	brubeck_histo_push(&h, 42.0);

	brubeck_histo_sample(&sample, &h);

	sput_fail_unless(sample.min == 42.0, 0);
	sput_fail_unless(sample.max == 1.3e12, 0);
	sput_fail_unless(sample.median == 42.0, 0);
}
