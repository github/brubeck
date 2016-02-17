#include "brubeck.h"

#ifdef BRUBECK_HAVE_MICROHTTPD
#include "microhttpd.h"
#include "jansson.h"

static struct brubeck_metric *safe_lookup_metric(struct brubeck_server *server, const char *key)
{
	return brubeck_hashtable_find(server->metrics, key, (uint16_t)strlen(key));
}

static struct MHD_Response *
expire_metric(struct brubeck_server *server, const char *url)
{
	struct brubeck_metric *metric = safe_lookup_metric(
			server, url + strlen("/expire/"));

	if (metric) {
		metric->expire = BRUBECK_EXPIRE_DISABLED;
		return MHD_create_response_from_data(
				0, "", 0, 0);
	}
	return NULL;
}

static struct MHD_Response *
send_metric(struct brubeck_server *server, const char *url)
{
	static const char *metric_types[] = {
		"gauge", "meter", "counter", "histogram", "timer", "internal"
	};
	static const char *expire_status[] = {
		"disabled", "inactive", "active"
	};

	struct brubeck_metric *metric = safe_lookup_metric(
			server, url + strlen("/metric/"));

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

		char *jsonr = json_dumps(mj, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
		json_decref(mj);
		return MHD_create_response_from_data(
				strlen(jsonr), jsonr, 1, 0);
	}

	return NULL;
}

static struct MHD_Response *
send_stats(struct brubeck_server *brubeck)
{
	char *jsonr;
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
		} else if (backend->type == BRUBECK_BACKEND_OPENTSDB) {
                        struct brubeck_opentsdb *opentsdb = (struct brubeck_opentsdb *)backend;
                        struct sockaddr_in *address = &opentsdb->out_sockaddr;
                        char addr[INET_ADDRSTRLEN];

                        json_array_append_new(backends,
                                json_pack("{s:s, s:i, s:b, s:s, s:i, s:I}",
                                        "type", "opentsdb",
                                        "sample_freq", (int)opentsdb->backend.sample_freq,
                                        "connected", (opentsdb->out_sock >= 0),
                                        "address", inet_ntop(AF_INET, &address->sin_addr.s_addr, addr, INET_ADDRSTRLEN),
                                        "port", (int)ntohs(address->sin_port),
                                        "sent", (json_int_t)opentsdb->sent
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

	jsonr = json_dumps(stats, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
	json_decref(stats);
	return MHD_create_response_from_data(
		strlen(jsonr), jsonr, 1, 0);
}

static int
handle_request(void *cls, struct MHD_Connection *connection,
		const char *url, const char *method,
		const char *version, const char *upload_data,
		size_t *upload_data_size, void **con_cls)
{
	int ret;
	struct MHD_Response *response = NULL;
	struct brubeck_server *brubeck = cls;

	if (!strcmp(method, "GET")) {
		if (!strcmp(url, "/ping")) {
			char *jsonr;
			json_t *pong = json_pack("{s:s, s:i, s:s}",
				"version", "brubeck " GIT_SHA,
				"pid", (int)getpid(),
				"status", "OK");

			jsonr = json_dumps(pong, JSON_PRESERVE_ORDER);
			response = MHD_create_response_from_data(strlen(jsonr), jsonr, 1, 0);
			json_decref(pong);
		}

		else if (!strcmp(url, "/stats"))
			response = send_stats(brubeck);

		else if (starts_with(url, "/metric/"))
			response = send_metric(brubeck, url);
	}
	else if (!strcmp(method, "POST")) {
		if (starts_with(url, "/expire/"))
			response = expire_metric(brubeck, url);
	}

	if (!response) {
		static const char *NOT_FOUND = "404 not found";
		response = MHD_create_response_from_data(
			strlen(NOT_FOUND), (void *)NOT_FOUND, 0, 0);
		MHD_add_response_header(response, "Connection", "close");
		ret = MHD_queue_response(connection, 404, response);
	} else {
		MHD_add_response_header(response, "Connection", "close");
		MHD_add_response_header(response, "Content-Type", "application/json");
		ret = MHD_queue_response(connection, 200, response);
	}

	MHD_destroy_response(response);
	return ret;
}

void brubeck_http_endpoint_init(struct brubeck_server *server, const char *listen)
{
	struct MHD_Daemon *daemon;

	const char *port = strrchr(listen, ':');
	port = port ? port + 1 : listen;

	daemon = MHD_start_daemon(
			MHD_USE_SELECT_INTERNALLY,
			atoi(port),
			NULL, NULL,
			&handle_request, server,
			MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)10,
			MHD_OPTION_END);

	if (!daemon)
		die("failed to start HTTP endpoint");
	log_splunk("event=http_server listen=%s", port);
}

#else

void brubeck_http_endpoint_init(struct brubeck_server *server, const char *listen)
{
	die("http support has not been compiled in Brubeck");
}

#endif

