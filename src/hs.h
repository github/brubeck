#ifndef _HS_H_
#define _HS_H_

#include <stdbool.h>
#include "ck_hs.h"

typedef struct ck_hs brubeck_hashset_t;

brubeck_hashset_t * brubeck_hashset_new(const uint64_t size);
void brubeck_hashset_free(brubeck_hashset_t *hs);
bool brubeck_hashset_add(brubeck_hashset_t *hs, const char *key);
bool brubeck_hashset_clear(brubeck_hashset_t *hs);

#endif
