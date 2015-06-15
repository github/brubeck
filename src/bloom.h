#ifndef __BLOOM_FILTER_H
#define __BLOOM_FILTER_H

#include <stdint.h>

struct multibloom {
	int bits;
	int bytes;
	int hashes;
	unsigned char *filters[];
};

int multibloom_check(struct multibloom *bloom, int f, uint32_t a, uint32_t b);
void multibloom_reset(struct multibloom *bloom, int f);
struct multibloom *multibloom_new(int filters, int entries, double error);

#endif
