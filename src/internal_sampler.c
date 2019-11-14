#include "brubeck.h"

#define INTERNAL_LONGEST_KEY ".secure.from_future"

void
brubeck_internal__sample(struct brubeck_metric *metric, brubeck_sample_cb sample, void *opaque)
{
	struct brubeck_internal_stats *stats = metric->as.other;
	uint32_t value;

	char *key = alloca(metric->key_len + strlen(INTERNAL_LONGEST_KEY) + 1);
	memcpy(key, metric->key, metric->key_len);

	WITH_SUFFIX(".metrics") {
		value = brubeck_atomic_swap(&stats->live.metrics, 0);
		stats->sample.metrics = value;
		sample(key, (value_t)value, opaque);
	}

	WITH_SUFFIX(".errors") {
		value = brubeck_atomic_swap(&stats->live.errors, 0);
		stats->sample.errors = value;
		sample(key, (value_t)value, opaque);
	}

	WITH_SUFFIX(".unique_keys") {
		value = brubeck_atomic_fetch(&stats->live.unique_keys);
		stats->sample.unique_keys = value;
		sample(key, (value_t)value, opaque);
	}

	/* Secure statsd endpoint */
	WITH_SUFFIX(".secure.failed") {
		value = brubeck_atomic_swap(&stats->live.secure.failed, 0);
		stats->sample.secure.failed = value;
		sample(key, (value_t)value, opaque);
	}

	WITH_SUFFIX(".secure.from_future") {
		value = brubeck_atomic_swap(&stats->live.secure.from_future, 0);
		stats->sample.secure.from_future = value;
		sample(key, (value_t)value, opaque);
	}

	WITH_SUFFIX(".secure.delayed") {
		value = brubeck_atomic_swap(&stats->live.secure.delayed, 0);
		stats->sample.secure.delayed = value;
		sample(key, (value_t)value, opaque);
	}

	WITH_SUFFIX(".secure.replayed") {
		value = brubeck_atomic_swap(&stats->live.secure.replayed, 0);
		stats->sample.secure.replayed = value;
		sample(key, (value_t)value, opaque);
	}

	/*
	 * Mark the metric as active so it doesn't get disabled
	 * by the inactive metrics pruner
	 */
	metric->state = BRUBECK_STATE_ACTIVE;
}

void brubeck_internal__init(struct brubeck_server *server)
{
	struct brubeck_metric *internal;
	struct brubeck_backend *backend;

	internal = brubeck_metric_new(server,
			server->name, strlen(server->name),
			BRUBECK_MT_INTERNAL_STATS);

	if (internal == NULL)
		die("Failed to initialize internal stats sampler");

	internal->as.other = &server->internal_stats;

	backend = brubeck_metric_shard(server, internal);
	server->internal_stats.sample_freq = backend->sample_freq;
}
