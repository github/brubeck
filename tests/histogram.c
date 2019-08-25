#include "brubeck.h"
#include "sput.h"
#include "thread_helper.h"
#include <limits.h>

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
				brubeck_histo_push(&t->h, 0.42, 1.0);
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

	brubeck_histo_push(&h, 42.0, 1.0);
	sput_fail_unless(h.size == 1, "histogram size");
	sput_fail_unless(h.count == 1, "histogram value count");

	brubeck_histo_sample(&sample, &h);

	sput_fail_unless(sample.lower == 42.0, "sample.lower");
	sput_fail_unless(sample.upper == 42.0, "sample.upper");
	sput_fail_unless(sample.upper_90 == 42.0, "sample.upper_90");
	sput_fail_unless(sample.mean == 42.0, "sample.mean");
	sput_fail_unless(sample.mean_90 == 42.0, "sample.mean_90");
	sput_fail_unless(sample.count == 1, "sample.count");
	sput_fail_unless(sample.count_90 == 1.0, "sample.count_90");
	sput_fail_unless(sample.sum == 42.0, "sample.sum");
	sput_fail_unless(sample.sum_90 == 42.0, "sample.sum_90");
	sput_fail_unless(sample.std == 0.0, "sample.std");
	sput_fail_unless(sample.median == 42.0, "sample.median");
}

void test_histogram__large_range(void)
{
	struct brubeck_histo h;
	struct brubeck_histo_sample sample;

	memset(&h, 0x0, sizeof(h));

	brubeck_histo_push(&h, 1.3e12, 1.0);
	brubeck_histo_push(&h, 42.0, 1.0);
	brubeck_histo_push(&h, 42.0, 1.0);

	brubeck_histo_sample(&sample, &h);

	sput_fail_unless(sample.lower == 42.0, "sample.lower");
	sput_fail_unless(sample.upper == 1.3e12, "sample.upper");
	sput_fail_unless(sample.median == 42.0, "sample.median");
}

void test_histogram__multisamples(void)
{
	struct brubeck_histo h;
	struct brubeck_histo_sample sample;
	size_t i, j;

	memset(&h, 0x0, sizeof(h));

	for (i = 0; i < 8; ++i) {
		for (j = 0; j < 128; ++j)
			brubeck_histo_push(&h, (double)(j + 1), 1.0);

		sput_fail_unless(h.size == 128, "histogram size");
		sput_fail_unless(h.count == 128, "histogram value count");

		brubeck_histo_sample(&sample, &h);

		sput_fail_unless(sample.lower == 1.0, "sample.lower");
		sput_fail_unless(sample.upper == 128.0, "sample.upper");
		sput_fail_unless(sample.upper_90 == 115.0, "sample.upper_90");
		sput_fail_unless(sample.mean == 64.5, "sample.mean");
		sput_fail_unless(sample.mean_90 == 58.0, "sample.mean_90");
		sput_fail_unless(sample.count == 128, "sample.count");
		sput_fail_unless(sample.count_90 == 115, "sample.count_90");
		sput_fail_unless(sample.sum == 8256.0, "sample.sum");
		sput_fail_unless(sample.sum_90 == 6670, "sample.sum_90");
		sput_fail_unless(floor(sample.std) == 36.0, "sample.std");
		sput_fail_unless(sample.median == 64.0, "sample.median");
	}
}

void test_histogram__with_sample_rate(void)
{
	struct brubeck_histo h;
	struct brubeck_histo_sample sample;
	size_t j;

	memset(&h, 0x0, sizeof(h));

	for (j = 0; j < 128; ++j)
		brubeck_histo_push(&h, (double)(j + 1), 10.0);

	sput_fail_unless(h.size == 128, "histogram size");
	sput_fail_unless(h.count == 1280, "histogram value count");

	brubeck_histo_sample(&sample, &h);

	sput_fail_unless(sample.lower == 1.0, "sample.lower");
	sput_fail_unless(sample.upper == 128.0, "sample.upper");
	sput_fail_unless(sample.upper_90 == 115.0, "sample.upper_90");
	sput_fail_unless(sample.mean == 64.5, "sample.mean");
	sput_fail_unless(floor(sample.mean_90) == 57.0, "sample.mean_90");
	sput_fail_unless(sample.count == 1280, "sample.count");
	sput_fail_unless(sample.count_90 == 1152.0, "sample.count_90");
	sput_fail_unless(sample.sum == 82560.0, "sample.sum");
	sput_fail_unless(sample.sum_90 == 66700, "sample.sum_90");
	sput_fail_unless(floor(sample.std) == 36.0, "sample.std");
	sput_fail_unless(sample.median == 64.0, "sample.median");
}

void test_histogram__capacity(void)
{
	static const size_t HISTO_CAP = USHRT_MAX;

	struct brubeck_histo h;
	struct brubeck_histo_sample sample;
	size_t j;

	memset(&h, 0x0, sizeof(h));

	for (j = 0; j < HISTO_CAP + 500; ++j)
		brubeck_histo_push(&h, (double)(j + 1), 1.0);

	sput_fail_unless(h.size == HISTO_CAP, "histogram size");
	sput_fail_unless(h.count == (HISTO_CAP + 500), "histogram value count");

	brubeck_histo_sample(&sample, &h);

	sput_fail_unless(sample.lower == 1.0, "sample.lower");
	sput_fail_unless(sample.upper == (double)HISTO_CAP, "sample.upper");
	sput_fail_unless(sample.count == (HISTO_CAP + 500), "sample.count");

	for (j = 0; j < HISTO_CAP + 500; ++j)
		brubeck_histo_push(&h, (double)(j + 1), 10.0);

	sput_fail_unless(h.size == HISTO_CAP, "histogram size");
	sput_fail_unless(h.count == ((HISTO_CAP + 500) * 10), "histogram value count");

	brubeck_histo_sample(&sample, &h);

	sput_fail_unless(sample.lower == 1.0, "sample.lower");
	sput_fail_unless(sample.upper == (double)HISTO_CAP, "sample.upper");
	sput_fail_unless(sample.count == ((HISTO_CAP + 500) * 10), "sample.count");
}

