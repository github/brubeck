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
	size_t sent;
};

struct brubeck_backend *brubeck_carbon_new(
	struct brubeck_server *server, json_t *settings, int shard_n);

#endif
