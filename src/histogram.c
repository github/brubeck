#include "brubeck.h"
#include <limits.h>

#define HISTO_INIT_SIZE 16

void brubeck_histo_push(struct brubeck_histo *histo, value_t value, value_t sample_rate)
{
	histo->count += (1.0 / sample_rate);

	if (histo->size == histo->alloc) {
		size_t new_size;

		if (histo->size == USHRT_MAX)
			return;

		new_size = histo->alloc * 2;

		if (new_size > USHRT_MAX)
			new_size = USHRT_MAX;
		if (new_size < HISTO_INIT_SIZE)
			new_size = HISTO_INIT_SIZE;
		if (new_size != histo->alloc) {
			histo->alloc = (uint16_t)new_size;
			histo->values = xrealloc(histo->values, histo->alloc * sizeof(value_t));
		}
	}

	histo->values[histo->size++] = value;
}

static inline value_t histo_percentile(struct brubeck_histo *histo, float rank)
{
	size_t irank = floor((rank * histo->size) + 0.5f);
	return histo->values[irank - 1];
}

static inline value_t histo_sum(struct brubeck_histo *histo)
{
	size_t i;
	value_t sum = 0.0;

	for (i = 0; i < histo->size; ++i)
		sum += histo->values[i];

	return sum;
}

static int value_cmp(const void *a, const void *b)
{
	const value_t va = *(const value_t *)a, vb = *(const value_t *)b;
	if (va < vb)
		return -1;
	if (va > vb)
		return 1;
	return 0;
}

static inline void histo_sort(struct brubeck_histo *histo)
{
	qsort(histo->values, histo->size, sizeof(value_t), &value_cmp);
}

void brubeck_histo_sample(
		struct brubeck_histo_sample *sample,
		struct brubeck_histo *histo)
{
	if (histo->size == 0) {
		memset(sample, 0x0, sizeof(struct brubeck_histo_sample));
		return;
	}

	histo_sort(histo);

	sample->sum = histo_sum(histo);
	sample->min = histo->values[0];
	sample->max = histo->values[histo->size - 1];
	sample->mean = sample->sum / histo->size;
	sample->median = histo_percentile(histo, 0.5f);
	sample->count = histo->count;

	sample->percentile[PC_75] = histo_percentile(histo, 0.75f);
	sample->percentile[PC_95] = histo_percentile(histo, 0.95f);
	sample->percentile[PC_98] = histo_percentile(histo, 0.98f);
	sample->percentile[PC_99] = histo_percentile(histo, 0.99f);
	sample->percentile[PC_999] = histo_percentile(histo, 0.999f);

	/* empty the histogram */
	histo->size = 0;
}
