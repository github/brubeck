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

enum brubeck_aggregate_t {
	BRUBECK_AG_LAST,
	BRUBECK_AG_SUM,
	BRUBECK_AG_MIN,
	BRUBECK_AG_MAX,
	BRUBECK_AG_AVERAGE
};

enum {
	BRUBECK_EXPIRE_DISABLED = 0,
	BRUBECK_EXPIRE_INACTIVE = 1,
	BRUBECK_EXPIRE_ACTIVE = 2
};

struct brubeck_metric {
	struct brubeck_metric *next;

	pthread_spinlock_t lock;
	uint16_t key_len;
	uint8_t type;
	uint8_t expire;

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
	const struct brubeck_metric *metric,
	const char *key,
	value_t value,
	void *backend);

void brubeck_metric_sample(struct brubeck_metric *metric, brubeck_sample_cb cb, void *backend);
void brubeck_metric_record(struct brubeck_metric *metric, value_t value);

struct brubeck_metric *brubeck_metric_new(struct brubeck_server *server, const char *, size_t, uint8_t);
struct brubeck_metric *brubeck_metric_find(struct brubeck_server *server, const char *, size_t, uint8_t);

#define WITH_SUFFIX(suffix) memcpy(key + metric->key_len, suffix, strlen(suffix) + 1);

#endif


