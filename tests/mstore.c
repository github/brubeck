#include "sput.h"
#include "brubeck.h"

static struct brubeck_metric *new_metric(const char *name)
{
	size_t name_len = strlen(name);
	struct brubeck_metric *metric = malloc(sizeof(struct brubeck_metric) + name_len + 1);

	memcpy(metric->key, name, name_len);
	metric->key[name_len] = 0;
	metric->key_len = name_len;

	return metric;
}

void test_mstore__save(void)
{
	static const int nmetrics = 15000;
	brubeck_hashtable_t *store;
	int i;

	store = brubeck_hashtable_new(4096);

	for (i = 0; i < nmetrics; ++i) {
		char buffer[64];
		struct brubeck_metric *metric;

		sprintf(buffer, "github.test.metric.%d", i);
		metric = new_metric(buffer);

		if (!brubeck_hashtable_insert(store, metric->key, metric->key_len, metric))
			break;
	}

	sput_fail_unless(i == nmetrics, "stored 15000 metrics in table");

	for (i = 0; i < nmetrics; ++i) {
		char buffer[64];
		uint16_t len;

		len = sprintf(buffer, "github.test.metric.%d", i);

		if (!brubeck_hashtable_find(store, buffer, len))
			break;
	}

	sput_fail_unless(i == nmetrics, "lookup all metrics from table");
}
