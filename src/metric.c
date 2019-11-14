#include "brubeck.h"

static inline struct brubeck_metric *
new_metric(struct brubeck_server *server, const char *key, size_t key_len, uint8_t type)
{
	struct brubeck_metric *metric;

	/* slab allocation cannot fail */
	metric = brubeck_slab_alloc(&server->slab,
		sizeof(struct brubeck_metric) + key_len + 1);

	memset(metric, 0x0, sizeof(struct brubeck_metric));

	memcpy(metric->key, key, key_len);
	metric->key[key_len] = '\0';
	metric->key_len = (uint16_t)key_len;

	metric->state = BRUBECK_STATE_ACTIVE;
	metric->type = type;
	pthread_spin_init(&metric->lock, PTHREAD_PROCESS_PRIVATE);

#ifdef BRUBECK_METRICS_FLOW
	metric->flow = 0;
#else
	/* Compile time assert: ensure that the metric struct can be packed
	 * in a single slab */
	ct_assert(sizeof(struct brubeck_metric) <= (SLAB_SIZE));
#endif

	return metric;
}

typedef void (*mt_prototype_record)(struct brubeck_metric *, value_t, value_t, uint8_t);
typedef void (*mt_prototype_sample)(struct brubeck_metric *, brubeck_sample_cb, void *);


/*********************************************
 * Gauge
 *
 * ALLOC: mt + 4 bytes
 *********************************************/
static void
gauge__record(struct brubeck_metric *metric, value_t value, value_t sample_freq, uint8_t modifiers)
{
	pthread_spin_lock(&metric->lock);
	{
		if (modifiers & BRUBECK_MOD_RELATIVE_VALUE) {
			metric->as.gauge.value += value;
		} else {
			metric->as.gauge.value = value;
		}
		metric->state= BRUBECK_STATE_ACTIVE;
	}
	pthread_spin_unlock(&metric->lock);
}

static void
gauge__sample(struct brubeck_metric *metric, brubeck_sample_cb sample, void *opaque)
{
	value_t value;

	pthread_spin_lock(&metric->lock);
	{
		value = metric->as.gauge.value;
		metric->state = BRUBECK_STATE_INACTIVE;
	}
	pthread_spin_unlock(&metric->lock);

	sample(metric->key, value, opaque);
}


/*********************************************
 * Meter
 *
 * ALLOC: mt + 4
 *********************************************/
static void
meter__record(struct brubeck_metric *metric, value_t value, value_t sample_freq, uint8_t modifiers)
{
	/* upsample */
	value *= sample_freq;

	pthread_spin_lock(&metric->lock);
	{
		metric->as.meter.value += value;
		metric->state= BRUBECK_STATE_ACTIVE;
	}
	pthread_spin_unlock(&metric->lock);
}

static void
meter__sample(struct brubeck_metric *metric, brubeck_sample_cb sample, void *opaque)
{
	value_t value;

	pthread_spin_lock(&metric->lock);
	{
		value = metric->as.meter.value;
		metric->as.meter.value = 0.0;
		metric->state = BRUBECK_STATE_INACTIVE;
	}
	pthread_spin_unlock(&metric->lock);

	sample(metric->key, value, opaque);
}


/*********************************************
 * Counter
 *
 * ALLOC: mt + 4 + 4 + 4
 *********************************************/
static void
counter__record(struct brubeck_metric *metric, value_t value, value_t sample_freq, uint8_t modifiers)
{
	/* upsample */
	value *= sample_freq;

	pthread_spin_lock(&metric->lock);
	{
		if (metric->as.counter.previous > 0.0) {
			value_t diff = (value >= metric->as.counter.previous) ?
				(value - metric->as.counter.previous) :
				(value);

			metric->as.counter.value += diff;
		}

		metric->as.counter.previous = value;
		metric->state= BRUBECK_STATE_ACTIVE;
	}
	pthread_spin_unlock(&metric->lock);
}

static void
counter__sample(struct brubeck_metric *metric, brubeck_sample_cb sample, void *opaque)
{
	value_t value;

	pthread_spin_lock(&metric->lock);
	{
		value = metric->as.counter.value;
		metric->as.counter.value = 0.0;
		metric->state = BRUBECK_STATE_INACTIVE;
	}
	pthread_spin_unlock(&metric->lock);

	sample(metric->key, value, opaque);
}


/*********************************************
 * Histogram / Timer
 *
 * ALLOC: mt + 16 + 4
 *********************************************/
static void
histogram__record(struct brubeck_metric *metric, value_t value, value_t sample_freq, uint8_t modifiers)
{
	pthread_spin_lock(&metric->lock);
	{
		brubeck_histo_push(&metric->as.histogram, value, sample_freq);
		metric->state= BRUBECK_STATE_ACTIVE;
	}
	pthread_spin_unlock(&metric->lock);
}

static void
histogram__sample(struct brubeck_metric *metric, brubeck_sample_cb sample, void *opaque)
{
	struct brubeck_histo_sample hsample;
	char *key;

	pthread_spin_lock(&metric->lock);
	{
		brubeck_histo_sample(&hsample, &metric->as.histogram);
		metric->state = BRUBECK_STATE_INACTIVE;
	}
	pthread_spin_unlock(&metric->lock);

	/* alloc space for this on the stack. we need enough for:
	 * key_length + longest_suffix + null terminator
	 */
	key = alloca(metric->key_len + strlen(".percentile.999") + 1);
	memcpy(key, metric->key, metric->key_len);


	WITH_SUFFIX(".count") {
		sample(key, hsample.count, opaque);
	}

	WITH_SUFFIX(".count_ps") {
		struct brubeck_backend *backend = opaque;
		sample(key, hsample.count / (double)backend->sample_freq, opaque);
	}

	WITH_SUFFIX(".min") {
		sample(key, hsample.min, opaque);
	}

	WITH_SUFFIX(".max") {
		sample(key, hsample.max, opaque);
	}

	WITH_SUFFIX(".sum") {
		sample(key, hsample.sum, opaque);
	}

	WITH_SUFFIX(".mean") {
		sample(key, hsample.mean, opaque);
	}

	WITH_SUFFIX(".median") {
		sample(key, hsample.median, opaque);
	}

	WITH_SUFFIX(".percentile.75") {
		sample(key, hsample.percentile[PC_75], opaque);
	}

	WITH_SUFFIX(".percentile.95") {
		sample(key, hsample.percentile[PC_95], opaque);
	}

	WITH_SUFFIX(".percentile.98") {
		sample(key, hsample.percentile[PC_98], opaque);
	}

	WITH_SUFFIX(".percentile.99") {
		sample(key, hsample.percentile[PC_99], opaque);
	}

	WITH_SUFFIX(".percentile.999") {
		sample(key, hsample.percentile[PC_999], opaque);
	}
}

/********************************************************/

static struct brubeck_metric__proto {
	mt_prototype_record record;
	mt_prototype_sample sample;
} _prototypes[] = {
	/* Gauge */
	{
		&gauge__record,
		&gauge__sample
	},

	/* Meter */
	{
		&meter__record,
		&meter__sample
	},

	/* Counter */
	{
		&counter__record,
		&counter__sample
	},

	/* Histogram */
	{
		&histogram__record,
		&histogram__sample
	},

	/* Timer -- uses same implementation as histogram */
	{
		&histogram__record,
		&histogram__sample
	},

	/* Internal -- used for sampling brubeck itself */
	{
		NULL, /* recorded manually */
		brubeck_internal__sample
	}
};

void brubeck_metric_sample(struct brubeck_metric *metric, brubeck_sample_cb cb, void *backend)
{
	_prototypes[metric->type].sample(metric, cb, backend);
}

void brubeck_metric_record(struct brubeck_metric *metric, value_t value, value_t sample_freq, uint8_t modifiers)
{
	_prototypes[metric->type].record(metric, value, sample_freq, modifiers);
}

struct brubeck_backend *
brubeck_metric_shard(struct brubeck_server *server, struct brubeck_metric *metric)
{
	int shard = 0;
	if (server->active_backends > 1)
		shard = CityHash32(metric->key, metric->key_len) % server->active_backends;
	return server->backends[shard];
}

struct brubeck_metric *
brubeck_metric_new(struct brubeck_server *server, const char *key, size_t key_len, uint8_t type)
{
	struct brubeck_metric *metric;

	metric = new_metric(server, key, key_len, type);
	if (!metric)
		return NULL;

	if (!brubeck_hashtable_insert(server->metrics, metric->key, metric->key_len, metric))
		return brubeck_hashtable_find(server->metrics, key, key_len);

	brubeck_backend_register_metric(brubeck_metric_shard(server, metric), metric);

	/* Record internal stats */
	brubeck_stats_inc(server, unique_keys);
	return metric;
}

struct brubeck_metric *
brubeck_metric_find(struct brubeck_server *server, const char *key, size_t key_len, uint8_t type)
{
	struct brubeck_metric *metric;

	assert(key[key_len] == '\0');
	metric = brubeck_hashtable_find(server->metrics, key, (uint16_t)key_len);

	if (unlikely(metric == NULL)) {
		if (server->at_capacity)
			return NULL;

		return brubeck_metric_new(server, key, key_len, type);
	}

#ifdef BRUBECK_METRICS_FLOW
	brubeck_atomic_inc(&metric->flow);
#endif

	return metric;
}
