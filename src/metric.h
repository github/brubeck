#ifndef __BRUBECK_METRIC_H__
#define __BRUBECK_METRIC_H__

enum brubeck_metric_t {
	BRUBECK_MT_GAUGE, /** g */
	BRUBECK_MT_METER, /** c */
	BRUBECK_MT_COUNTER, /** C */
	BRUBECK_MT_HISTO, /** h */
	BRUBECK_MT_TIMER, /** ms */
	BRUBECK_MT_INTERNAL_STATS
};

enum brubeck_metric_mod_t {
	BRUBECK_MOD_RELATIVE_VALUE = 1
};

enum brubeck_aggregate_t {
	BRUBECK_AG_LAST,
	BRUBECK_AG_SUM,
	BRUBECK_AG_MIN,
	BRUBECK_AG_MAX,
	BRUBECK_AG_AVERAGE
};

enum {
	BRUBECK_STATE_INACTIVE = 0,
	BRUBECK_STATE_ACTIVE = 1
};

struct brubeck_metric {
	struct brubeck_metric *next;

#ifdef BRUBECK_METRICS_FLOW
	uint64_t flow;
#endif

	pthread_spinlock_t lock;
	uint16_t key_len;
	uint8_t type;
	uint8_t state;

	union {
		struct {
			value_t value;
		} gauge, meter;
		struct {
			value_t value, previous;
		} counter;
		struct brubeck_histo histogram;
		void *other;
	} as;

	char key[];
};

typedef void (*brubeck_sample_cb)(
	const char *key,
	value_t value,
	void *backend);

void brubeck_metric_sample(struct brubeck_metric *metric, brubeck_sample_cb cb, void *backend);
void brubeck_metric_record(struct brubeck_metric *metric, value_t value, value_t sample_rate, uint8_t modifiers);

struct brubeck_metric *brubeck_metric_new(struct brubeck_server *server, const char *, size_t, uint8_t);
struct brubeck_metric *brubeck_metric_find(struct brubeck_server *server, const char *, size_t, uint8_t);
struct brubeck_backend *brubeck_metric_shard(struct brubeck_server *server, struct brubeck_metric *);

#define WITH_SUFFIX(suffix) memcpy(key + metric->key_len, suffix, strlen(suffix) + 1);

#endif


