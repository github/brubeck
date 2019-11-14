#ifndef __BRUBECK_BACKEND_H__
#define __BRUBECK_BACKEND_H__

enum brubeck_backend_t {
	BRUBECK_BACKEND_CARBON,
        BRUBECK_BACKEND_OPENTSDB
};

struct brubeck_backend {
	enum brubeck_backend_t type;
	struct brubeck_server *server;
	int sample_freq;
	int shard_n;

	int (*connect)(void *);
	bool (*is_connected)(void *);
	void (*sample)(const char *, value_t, void *);
	void (*flush)(void *);

	uint32_t tick_time;
	pthread_t thread;

	struct brubeck_metric *queue;
};

void brubeck_backend_run_threaded(struct brubeck_backend *);
void brubeck_backend_register_metric(struct brubeck_backend *self, struct brubeck_metric *metric);

static inline const char *brubeck_backend_name(struct brubeck_backend *backend)
{
	switch (backend->type) {
		case BRUBECK_BACKEND_CARBON: return "carbon";
                case BRUBECK_BACKEND_OPENTSDB: return "opentsdb";
		default: return NULL;
	}
}

#include "backends/carbon.h"
#include "backends/opentsdb.h"

#endif
