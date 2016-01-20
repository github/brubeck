#ifndef _HT_H_
#define _HT_H_

#include <stdbool.h>

struct brubeck_metric;
typedef struct brubeck_hashtable_t brubeck_hashtable_t;

brubeck_hashtable_t *brubeck_hashtable_new(const uint64_t size);
void brubeck_hashtable_free(brubeck_hashtable_t *ht);
struct brubeck_metric *brubeck_hashtable_find(brubeck_hashtable_t *ht, const char *key, uint16_t key_len);
bool brubeck_hashtable_insert(brubeck_hashtable_t *ht, const char *key, uint16_t key_len, struct brubeck_metric *val);
size_t brubeck_hashtable_size(brubeck_hashtable_t *ht);
void brubeck_hashtable_foreach(brubeck_hashtable_t *ht, void (*callback)(struct brubeck_metric *, void *), void *payload);
struct brubeck_metric **brubeck_hashtable_to_a(brubeck_hashtable_t *ht, size_t *length);

#endif
