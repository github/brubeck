#include <string.h>
#include <stdint.h>

#ifdef _MSC_VER
#	include <stdlib.h>
#	define bswap_32(x) _byteswap_ulong(x)
#	define bswap_64(x) _byteswap_uint64(x)
#elif defined(__APPLE__)
#	include <libkern/OSByteOrder.h>
#	define bswap_32(x) OSSwapInt32(x)
#	define bswap_64(x) OSSwapInt64(x)
#else
#	include <byteswap.h>
#endif

static uint32_t read32(const char *p)
{
	uint32_t result;
	memcpy(&result, p, sizeof(result));
	return result;
}

// Magic numbers for 32-bit hashing.  Copied from murmur3.
static const uint32_t c1 = 0xcc9e2d51;
static const uint32_t c2 = 0x1b873593;

// A 32-bit to 32-bit integer hash copied from murmur3.
static uint32_t fmix(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

static uint32_t ror32(uint32_t val, int shift)
{
	return shift == 0 ? val : ((val >> shift) | (val << (32 - shift)));
}

#define swap(x, y) do { typeof(x) aux = x; x = y; y = aux; } while (0)

#define PERMUTE3(a, b, c) do { swap(a, b); swap(a, c); } while (0)

static uint32_t mur(uint32_t a, uint32_t h)
{
	// Helper from murmur3 for combining two 32-bit values.
	a *= c1;
	a = ror32(a, 17);
	a *= c2;
	h ^= a;
	h = ror32(h, 19);
	return h * 5 + 0xe6546b64;
}

static uint32_t Hash32Len13to24(const char *s, size_t len)
{
	uint32_t a = read32(s - 4 + (len >> 1));
	uint32_t b = read32(s + 4);
	uint32_t c = read32(s + len - 8);
	uint32_t d = read32(s + (len >> 1));
	uint32_t e = read32(s);
	uint32_t f = read32(s + len - 4);
	uint32_t h = len;

	return fmix(mur(f, mur(e, mur(d, mur(c, mur(b, mur(a, h)))))));
}

static uint32_t Hash32Len0to4(const char *s, size_t len)
{
	uint32_t b = 0;
	uint32_t c = 9;
	int i;

	for (i = 0; i < len; i++) {
		b = b * c1 + s[i];
		c ^= b;
	}
	return fmix(mur(b, mur(len, c)));
}

static uint32_t Hash32Len5to12(const char *s, size_t len)
{
	uint32_t a = len, b = len * 5, c = 9, d = b;
	a += read32(s);
	b += read32(s + len - 4);
	c += read32(s + ((len >> 1) & 4));
	return fmix(mur(c, mur(b, mur(a, d))));
}

uint32_t CityHash32(const char *s, size_t len)
{
	size_t iters;
	uint32_t a0, a1, a2, a3, a4;
	uint32_t h, g, f;

	if (len <= 24) {
		return len <= 12 ?
			(len <= 4 ? Hash32Len0to4(s, len) : Hash32Len5to12(s, len)) :
			Hash32Len13to24(s, len);
	}

	h = len;
	g = c1 * len;
	f = g;

	a0 = ror32(read32(s + len - 4) * c1, 17) * c2;
	a1 = ror32(read32(s + len - 8) * c1, 17) * c2;
	a2 = ror32(read32(s + len - 16) * c1, 17) * c2;
	a3 = ror32(read32(s + len - 12) * c1, 17) * c2;
	a4 = ror32(read32(s + len - 20) * c1, 17) * c2;

	h ^= a0;
	h = ror32(h, 19);
	h = h * 5 + 0xe6546b64;
	h ^= a2;
	h = ror32(h, 19);
	h = h * 5 + 0xe6546b64;
	g ^= a1;
	g = ror32(g, 19);
	g = g * 5 + 0xe6546b64;
	g ^= a3;
	g = ror32(g, 19);
	g = g * 5 + 0xe6546b64;
	f += a4;
	f = ror32(f, 19);
	f = f * 5 + 0xe6546b64;

	iters = (len - 1) / 20;
	do {
		uint32_t a0 = ror32(read32(s) * c1, 17) * c2;
		uint32_t a1 = read32(s + 4);
		uint32_t a2 = ror32(read32(s + 8) * c1, 17) * c2;
		uint32_t a3 = ror32(read32(s + 12) * c1, 17) * c2;
		uint32_t a4 = read32(s + 16);
		h ^= a0;
		h = ror32(h, 18);
		h = h * 5 + 0xe6546b64;
		f += a1;
		f = ror32(f, 19);
		f = f * c1;
		g += a2;
		g = ror32(g, 18);
		g = g * 5 + 0xe6546b64;
		h ^= a3 + a1;
		h = ror32(h, 19);
		h = h * 5 + 0xe6546b64;
		g ^= a4;
		g = bswap_32(g) * 5;
		h += a4 * 5;
		h = bswap_32(h);
		f += a0;
		PERMUTE3(f, h, g);
		s += 20;
	} while (--iters != 0);
	g = ror32(g, 11) * c1;
	g = ror32(g, 17) * c1;
	f = ror32(f, 11) * c1;
	f = ror32(f, 17) * c1;
	h = ror32(h + g, 19);
	h = h * 5 + 0xe6546b64;
	h = ror32(h, 17) * c1;
	h = ror32(h + f, 19);
	h = h * 5 + 0xe6546b64;
	h = ror32(h, 17) * c1;
	return h;
}
