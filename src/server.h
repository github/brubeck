#ifndef __BRUBECK_SERVER_H__
#define __BRUBECK_SERVER_H__

struct brubeck_internal_stats {
	int sample_freq;
	struct {
		uint32_t metrics;
		uint32_t errors;
		uint32_t unique_keys;

		struct {
			uint32_t failed;
			uint32_t from_future;
			uint32_t delayed;
			uint32_t replayed;
		} secure;
	} live, sample;
};

// Server
struct brubeck_server {
	const char *name;
	const char *dump_path;
	const char *config_name;
	int running;
	int active_backends;
	int active_samplers;

	int fd_signal;
	int fd_expire;
	int fd_update;

	struct brubeck_slab slab;

	brubeck_hashtable_t *metrics;
	int at_capacity;

	struct brubeck_sampler *samplers[8];
	struct brubeck_backend *backends[8];

	json_t *config;
	struct brubeck_internal_stats internal_stats;
};

#define brubeck_stats_inc(server, STAT) brubeck_atomic_inc(&server->internal_stats.live.STAT)
#define brubeck_stats_sample(server, STAT) (server->internal_stats.sample.STAT)

void brubeck_http_endpoint_init(struct brubeck_server *server, const char *listen_on);

void brubeck_internal__init(struct brubeck_server *server);
void brubeck_internal__sample(struct brubeck_metric *metric, brubeck_sample_cb sample, void *opaque);

void brubeck_server_new_metric(struct brubeck_server *server, struct brubeck_metric *metric);

int brubeck_server_run(struct brubeck_server *server);
void brubeck_server_init(struct brubeck_server *server, const char *config);
void brubeck_server_conf(struct brubeck_server *server, int argc, char *argv[]);

void brubeck_cache_load(struct brubeck_server *server);
void brubeck_cache_save(struct brubeck_server *server);

#endif
