#include <stdlib.h>

#include "brubeck.h"
#include "ck_ht.h"
#include "ck_malloc.h"

struct brubeck_hashtable_t {
	ck_ht_t table;
	pthread_mutex_t write_mutex;
};

static void *
ht_malloc(size_t r)
{
	return xmalloc(r);
}

static void
ht_free(void *p, size_t b, bool r)
{
	free(p);
}

static struct ck_malloc ALLOCATOR = {
	.malloc = ht_malloc,
	.free = ht_free
};

brubeck_hashtable_t *
brubeck_hashtable_new(const uint64_t size)
{
	brubeck_hashtable_t *ht = xmalloc(sizeof(brubeck_hashtable_t));
	pthread_mutex_init(&ht->write_mutex, NULL);

	if (!ck_ht_init(&ht->table, CK_HT_MODE_BYTESTRING,
		NULL, &ALLOCATOR, (uint64_t)size, 0xDEADBEEF)) {
		free(ht);
		return NULL;
	}

	return ht;
}

void
brubeck_hashtable_free(brubeck_hashtable_t *ht)
{
	/* no-op */
}

struct brubeck_metric *
brubeck_hashtable_find(brubeck_hashtable_t *ht, const char *key, uint16_t key_len)
{
	ck_ht_hash_t h;
	ck_ht_entry_t entry;

	ck_ht_hash(&h, &ht->table, key, key_len);
	ck_ht_entry_key_set(&entry, key, key_len);

	if (ck_ht_get_spmc(&ht->table, h, &entry))
		return ck_ht_entry_value(&entry);

	return NULL;
}

bool
brubeck_hashtable_insert(brubeck_hashtable_t *ht, const char *key, uint16_t key_len, struct brubeck_metric *val)
{
	ck_ht_hash_t h;
	ck_ht_entry_t entry;
	bool result;

	ck_ht_hash(&h, &ht->table, key, key_len);
	ck_ht_entry_set(&entry, h, key, key_len, val);

	pthread_mutex_lock(&ht->write_mutex);
	result = ck_ht_put_spmc(&ht->table, h, &entry);
	pthread_mutex_unlock(&ht->write_mutex);

	return result;
}

size_t
brubeck_hashtable_size(brubeck_hashtable_t *ht)
{
	size_t len;

	pthread_mutex_lock(&ht->write_mutex);
	len = ck_ht_count(&ht->table);
	pthread_mutex_unlock(&ht->write_mutex);

	return len;
}

void
brubeck_hashtable_foreach(brubeck_hashtable_t *ht, void (*callback)(struct brubeck_metric *, void *), void *payload)
{
	ck_ht_iterator_t iterator = CK_HT_ITERATOR_INITIALIZER;
	ck_ht_entry_t *entry;

	pthread_mutex_lock(&ht->write_mutex);

	while (ck_ht_next(&ht->table, &iterator, &entry))
		callback(ck_ht_entry_value(entry), payload);

	pthread_mutex_unlock(&ht->write_mutex);
}

struct brubeck_metric **
brubeck_hashtable_to_a(brubeck_hashtable_t *ht, size_t *length)
{
	ck_ht_iterator_t iterator = CK_HT_ITERATOR_INITIALIZER;
	ck_ht_entry_t *entry;
	struct brubeck_metric **array;
	size_t i = 0;

	pthread_mutex_lock(&ht->write_mutex);
	*length = ck_ht_count(&ht->table);
	array = xmalloc(*length * sizeof(void *));

	while (ck_ht_next(&ht->table, &iterator, &entry))
		array[i++] = ck_ht_entry_value(entry);

	pthread_mutex_unlock(&ht->write_mutex);

	assert(*length == i);
	return array;
}
