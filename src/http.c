#include "brubeck.h"
#include "http/mongoose.h"
#include "jansson.h"

static int dump_json(const char *buffer, size_t size, void *data)
{
	struct mg_connection *conn = data;
	mg_send_data(conn, buffer, (int)size);
	return 0;
}

static void send_headers(struct mg_connection *conn)
{
	mg_send_status(conn, 200);
	mg_send_header(conn, "Connection", "close");
	mg_send_header(conn, "Content-Type", "application/json");
}

static struct brubeck_metric *safe_lookup_metric(struct brubeck_server *server, const char *key)
{
	return brubeck_hashtable_find(server->metrics, key, (uint16_t)strlen(key));
}

static int expire_metric(struct mg_connection *conn)
{
	struct brubeck_server *server = conn->server_param;
	struct brubeck_metric *metric = safe_lookup_metric(
			server, conn->uri + strlen("/expire/"));

	if (metric) {
		metric->expire = BRUBECK_EXPIRE_DISABLED;
		mg_send_status(conn, 200);
		mg_send_header(conn, "Connection", "close");
		return MG_TRUE;
	}
	return MG_FALSE;
}

static int send_metric(struct mg_connection *conn)
{
	static const char *metric_types[] = {
		"gauge", "meter", "counter", "histogram", "timer", "internal"
	};
	static const char *expire_status[] = {
		"disabled", "inactive", "active"
	};

	struct brubeck_server *server = conn->server_param;
	struct brubeck_metric *metric = safe_lookup_metric(
			server, conn->uri + strlen("/metric/"));

	if (metric) {
		json_t *mj = json_pack("{s:s, s:s, s:i, s:s}",
			"key", metric->key,
			"type", metric_types[metric->type],
#if METRIC_SHARD_SPECIFIER
			"shard", (int)metric->shard,
#else
			"shard", 0,
#endif
			"expire", expire_status[metric->expire]
		);

		send_headers(conn);
		json_dump_callback(mj, &dump_json, (void *)conn,
				JSON_INDENT(4) | JSON_PRESERVE_ORDER);
		json_decref(mj);
		return MG_TRUE;
	}

	return MG_FALSE;
}

static int send_stats(struct mg_connection *conn)
{
	struct brubeck_server *brubeck = conn->server_param;
	json_t *stats, *secure, *backends, *samplers;
	int i;
	
	backends = json_array();

	for (i = 0; i < brubeck->active_backends; ++i) {
		struct brubeck_backend *backend = brubeck->backends[i];

		if (backend->type == BRUBECK_BACKEND_CARBON) {
			struct brubeck_carbon *carbon = (struct brubeck_carbon *)backend;
			struct sockaddr_in *address = &carbon->out_sockaddr;
			char addr[INET_ADDRSTRLEN];

			json_array_append_new(backends,
				json_pack("{s:s, s:i, s:b, s:s, s:i, s:I}",
						"type", "carbon",
						"sample_freq", (int)carbon->backend.sample_freq,
						"connected", (carbon->out_sock >= 0),
						"address", inet_ntop(AF_INET, &address->sin_addr.s_addr, addr, INET_ADDRSTRLEN),
						"port", (int)ntohs(address->sin_port),
						"sent", (json_int_t)carbon->sent
				));
		}
	}

	samplers = json_array();

	for (i = 0; i < brubeck->active_samplers; ++i) {
		struct brubeck_sampler *sampler = brubeck->samplers[i];
		struct sockaddr_in *address = &sampler->addr;
		char addr[INET_ADDRSTRLEN];
		const char *sampler_name = NULL;

		switch (sampler->type) {
		case BRUBECK_SAMPLER_STATSD: sampler_name = "statsd"; break;
		case BRUBECK_SAMPLER_STATSD_SECURE: sampler_name = "statsd_secure"; break;
		default: assert(0);
		}

		json_array_append_new(samplers,
			json_pack("{s:s, s:f, s:s, s:i}",
				"type", sampler_name,
				"sample_freq", (double)sampler->current_flow,
				"address", inet_ntop(AF_INET, &address->sin_addr.s_addr, addr, INET_ADDRSTRLEN),
				"port", (int)ntohs(address->sin_port)
			));
	}

	secure = json_pack("{s:i, s:i, s:i, s:i}",
		"failed", brubeck->stats.secure.failed,
		"from_future", brubeck->stats.secure.from_future,
		"delayed", brubeck->stats.secure.delayed,
		"replayed", brubeck->stats.secure.replayed
	);

	stats = json_pack("{s:s, s:i, s:i, s:i, s:i, s:o, s:o, s:o}",
		"version", "brubeck " GIT_SHA,
		"metrics", brubeck->stats.metrics,
		"errors", brubeck->stats.errors,
		"unique_keys", brubeck->stats.unique_keys,
		"memory", brubeck->stats.memory,
		"secure", secure,
		"backends", backends,
		"samplers", samplers);

	json_dump_callback(stats, &dump_json, (void *)conn,
		JSON_INDENT(4) | JSON_PRESERVE_ORDER);
	json_decref(stats);

	return MG_TRUE;
}

static int handle_request(struct mg_connection *conn)
{
	static const char *PONG_STR =
		"{\"version\" : \"brubeck %s\", \"pid\" : %d, \"status\" : \"%s\"}\n";

	if (!strcmp(conn->request_method, "GET")) {
		if (!strcmp(conn->uri, "/ping")) {
			struct brubeck_server *brubeck = conn->server_param;
			const char *status = "OK";

			if (brubeck->at_capacity)
				status = "CAPACITY";
			if (!brubeck->running)
				status = "SHUTDOWN";

			send_headers(conn);
			mg_printf_data(conn, PONG_STR, GIT_SHA, getpid(), status);
			return MG_TRUE;
		}

		if (!strcmp(conn->uri, "/stats"))
			return send_stats(conn);

		if (starts_with(conn->uri, "/metric/"))
			return send_metric(conn);
	}

	if (!strcmp(conn->request_method, "POST")) {
		if (starts_with(conn->uri, "/expire/"))
			return expire_metric(conn);
	}

	return MG_FALSE;
}

static int event_handler(struct mg_connection *conn, enum mg_event ev)
{
	switch (ev) {
	case MG_AUTH:
		return MG_TRUE;

	case MG_REQUEST:
		return handle_request(conn);

	default:
		return MG_FALSE;
	}
}

static void *stats_thread(void *payload)
{
	struct mg_server *server = payload;

	for (;;) {
		mg_poll_server(server, 1000);   // Infinite loop, Ctrl-C to stop
	}
	mg_destroy_server(&server);
	return NULL;
}

void brubeck_http_endpoint_init(struct brubeck_server *server, const char *listen)
{
	struct mg_server *mongoose = mg_create_server(server, event_handler);

	mg_set_option(mongoose, "listening_port", listen);
	pthread_create(&server->stats.thread, NULL, &stats_thread, mongoose);
}
