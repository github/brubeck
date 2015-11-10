#include "brubeck.h"

#define SET_INIT_SIZE 16

void brubeck_set_add(struct brubeck_metric *metric, const char *key) {
	if(NULL == metric->as.set)
		metric->as.set = brubeck_hashset_new(SET_INIT_SIZE);

	if(NULL != metric->as.set)
		brubeck_hashset_add(metric->as.set, key);
}

size_t
brubeck_set_size(brubeck_hashset_t *hs)
{
	return ck_hs_count(hs);
}

bool
brubeck_set_clear(brubeck_hashset_t *hs)
{
	return brubeck_hashset_clear(hs);
}
