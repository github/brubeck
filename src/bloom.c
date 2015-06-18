#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "brubeck.h"
#include "bloom.h"

int multibloom_check(struct multibloom *bloom, int f, uint32_t a, uint32_t b)
{
	unsigned char *filter = bloom->filters[f];

	int hits = 0;
	uint32_t x, i, byte, mask;
	unsigned char c;

	for (i = 0; i < bloom->hashes; i++) {
		x = (a + i*b) % bloom->bits;
		byte = x >> 3;
		c = filter[byte];
		mask = 1 << (x % 8);

		if (c & mask) {
			hits++;
		} else {
			filter[byte] = c | mask;
		}
	}

	return (hits == bloom->hashes);
}

void multibloom_reset(struct multibloom *bloom, int f)
{
	memset(bloom->filters[f], 0x0, bloom->bytes);
}

struct multibloom *multibloom_new(int filters, int entries, double error)
{
	const double bpe = -(log(error) / 0.480453013918201);
	int i;
	struct multibloom *bloom = xmalloc(sizeof(struct multibloom) + filters * sizeof(void *));

	assert(entries > 1 && error > 0.0);

	bloom->bits = (int)((double)entries * bpe);
	bloom->bytes = (bloom->bits / 8);

	if (bloom->bits % 8)
		bloom->bytes++;

	bloom->hashes = (int)ceil(0.693147180559945 * bpe);  // ln(2)

	for (i = 0; i < filters; ++i)
		bloom->filters[i] = xcalloc(1, bloom->bytes);

	log_splunk(
		"event=bloom_init entries=%d error=%f bits=%d bpe=%f "
		"bytes=%d hash_funcs=%d",
		entries, error, bloom->bits, bpe, bloom->bytes, bloom->hashes
	);

	return bloom;
}

