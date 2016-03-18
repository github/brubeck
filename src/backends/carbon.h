#ifndef __BRUBECK_CARBON_H__
#define __BRUBECK_CARBON_H__

#define MAX_PICKLE_SIZE 256
#define PICKLE_BUFFER_SIZE 4096
#define PICKLE1_SIZE(key_len) (32 + key_len)

struct brubeck_carbon {
	struct brubeck_backend backend;

	int out_sock;
	struct sockaddr_in out_sockaddr;
	struct pickler {
			char *ptr;
			uint16_t pos;
			uint16_t pt;
	} pickler;
	struct namespacing {
		char *global;
		size_t global_len;

		char *counter;
		size_t counter_len;

		char *timer;
		size_t timer_len;

		char *histo;
		size_t histo_len;

		char *gauge;
		size_t gauge_len;
	} namespacing;
	size_t sent;
};

struct brubeck_backend *brubeck_carbon_new(
	struct brubeck_server *server, json_t *settings, int shard_n);

#endif
