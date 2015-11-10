#ifndef __BRUBECK_SET_H__
#define __BRUBECK_SET_H__

void brubeck_set_add(struct brubeck_metric *metric, const char *key);
size_t brubeck_set_size(brubeck_hashset_t *hs);
bool brubeck_set_clear(brubeck_hashset_t *hs);

#endif
