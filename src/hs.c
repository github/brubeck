#include <stdlib.h>

#include "brubeck.h"
#include "../vendor/ck/src/ck_ht_hash.h"
#include "ck_malloc.h"

static unsigned long
hs_hash(const void *object, unsigned long seed)
{
	const char *c = object;
	return (unsigned long)MurmurHash64A(c, strlen(c), seed);
}

static bool
hs_compare(const void *previous, const void *compare)
{
	return strcmp(previous, compare) == 0;
}

brubeck_hashset_t *
brubeck_hashset_new(const uint64_t size)
{
	brubeck_hashset_t *hs = xmalloc(sizeof(brubeck_hashset_t));
	if (!ck_hs_init(hs, CK_HS_MODE_DIRECT, hs_hash, hs_compare, &ALLOCATOR,
			(uint64_t)size, 0xDD15EA5E)) {
		free(hs);
		return NULL;
	}

	return hs;
}

void
brubeck_hashset_free(brubeck_hashset_t *hs)
{
	/* no-op */
}

bool
brubeck_hashset_add(brubeck_hashset_t *hs, const char *key) {
	if(NULL != ck_hs_get(hs, CK_HS_HASH(hs, hs_hash, key), key))
		return true;

	return ck_hs_put(hs, CK_HS_HASH(hs, hs_hash, key), xstrdup(key));
}

bool
brubeck_hashset_clear(brubeck_hashset_t *hs) {
	ck_hs_iterator_t iterator = CK_HS_ITERATOR_INITIALIZER;
	void *key;
	while(ck_hs_next(hs, &iterator, &key))
		free(key);
	return ck_hs_reset(hs);
}
