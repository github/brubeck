#include "brubeck.h"
#include <limits.h>

#define HISTO_INIT_SIZE 16

void brubeck_histo_push(struct brubeck_histo *histo, value_t value, value_t sample_freq)
{
	histo->count += sample_freq;

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

static inline value_t histo_percentile_rank(struct brubeck_histo *histo, float rank)
{
    size_t irank = floor((rank * histo->size) + 0.5f);
    return irank;
}

static inline value_t histo_percentile(struct brubeck_histo *histo, float rank)
{
    size_t irank = histo_percentile_rank(histo, rank);
    return histo->values[irank - 1];
}

static inline value_t histo_sum(struct brubeck_histo *histo, size_t values_size)
{
    size_t i;
    value_t sum = 0.0;

    value_t rate = (value_t) histo->count / histo->size;

    for (i = 0; i < values_size; ++i)
        sum += histo->values[i] * rate;

    return sum;
}

static inline value_t histo_std(struct brubeck_histo *histo, value_t mean)
{
	size_t i;
	value_t sum_of_diffs = 0.0;

	for (i = 0; i < histo->size; ++i)
		sum_of_diffs += (histo->values[i] - mean) * (histo->values[i] - mean);

	return sqrt(sum_of_diffs / histo->size);
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

	value_t pct_rank = histo_percentile_rank(histo, 0.9f);

	sample->count = histo->count;
	sample->count_90 = floor((histo->count * 0.9) + 0.5f);
	sample->sum = histo_sum(histo, histo->size);
	sample->sum_90 = histo_sum(histo, pct_rank);
	sample->lower = histo->values[0];
	sample->upper = histo->values[histo->size - 1];
	sample->upper_90 = histo->values[(size_t)pct_rank - 1];
	sample->mean = sample->sum / histo->count;
	sample->mean_90 = sample->sum_90 / sample->count_90;
	sample->median = histo_percentile(histo, 0.5f);
	sample->std = histo_std(histo, sample->mean);

	/* empty the histogram */
	histo->size = 0;
	histo->count = 0;
}
