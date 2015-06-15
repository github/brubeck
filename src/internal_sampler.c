#include "brubeck.h"

#define INTERNAL_LONGEST_KEY ".secure.from_future"

void
brubeck_internal__sample(struct brubeck_metric *metric, brubeck_sample_cb sample, void *opaque)
{
	struct brubeck_internal_stats *stats = metric->as.other;
	value_t value;

	char *key = alloca(metric->key_len + strlen(INTERNAL_LONGEST_KEY) + 1);
	memcpy(key, metric->key, metric->key_len);

	WITH_SUFFIX(".metrics") {
		value = brubeck_atomic_swap(&stats->metrics, 0);
		sample(key, value, opaque);
	}

	WITH_SUFFIX(".errors") {
		value = brubeck_atomic_swap(&stats->errors, 0);
		sample(key, value, opaque);
	}

	WITH_SUFFIX(".unique_keys") {
		value = brubeck_atomic_fetch(&stats->unique_keys);
		sample(key, value, opaque);
	}

	WITH_SUFFIX(".memory") {
		value = brubeck_atomic_fetch(&stats->memory);
		sample(key, value, opaque);
	}

	/* Secure statsd endpoint */
	WITH_SUFFIX(".secure.failed") {
		value = brubeck_atomic_swap(&stats->secure.failed, 0);
		sample(key, value, opaque);
	}

	WITH_SUFFIX(".secure.from_future") {
		value = brubeck_atomic_swap(&stats->secure.from_future, 0);
		sample(key, value, opaque);
	}

	WITH_SUFFIX(".secure.delayed") {
		value = brubeck_atomic_swap(&stats->secure.delayed, 0);
		sample(key, value, opaque);
	}

	WITH_SUFFIX(".secure.replayed") {
		value = brubeck_atomic_swap(&stats->secure.replayed, 0);
		sample(key, value, opaque);
	}

	/*
	 * Mark the metric as active so it doesn't get disabled
	 * by the inactive metrics pruner
	 */
	metric->expire = BRUBECK_EXPIRE_ACTIVE;
}

void brubeck_internal__init(struct brubeck_server *server)
{
	struct brubeck_metric *internal;

	internal = brubeck_metric_new(server,
			server->name, strlen(server->name),
			BRUBECK_MT_INTERNAL_STATS);

	if (internal == NULL) {
		die("Failed to initialize internal stats sampler");
	}

	internal->as.other = &server->stats;
}
