#ifndef __BRUBECK_HISTO_H__
#define __BRUBECK_HISTO_H__

struct brubeck_histo {
	value_t *values;
	uint32_t count;
	uint16_t alloc, size;
};

struct brubeck_histo_sample {
	value_t count;
	value_t count_90;
	value_t lower;
	value_t mean;
	value_t mean_90;
	value_t median;
	value_t std;
	value_t sum;
	value_t sum_90;
	value_t upper;
	value_t upper_90;
};

void brubeck_histo_push(struct brubeck_histo *histo, value_t value, value_t sample_rate);
void brubeck_histo_sample(
		struct brubeck_histo_sample *sample,
		struct brubeck_histo *histo);

#endif
