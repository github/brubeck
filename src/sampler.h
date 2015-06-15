#ifndef __BRUBECK_SAMPLER_H__
#define __BRUBECK_SAMPLER_H__

enum brubeck_sampler_t {
	BRUBECK_SAMPLER_STATSD,
	BRUBECK_SAMPLER_STATSD_SECURE,
};

struct brubeck_sampler {
	enum brubeck_sampler_t type;
	struct brubeck_server *server;

	int in_sock;
	struct sockaddr_in addr;

	size_t inflow;
	size_t current_flow;

	void (*shutdown)(struct brubeck_sampler *);
};

int brubeck_sampler_socket(struct brubeck_sampler *sampler, int multisock);
void brubeck_sampler_init_inet(
	struct brubeck_sampler *sampler,
	struct brubeck_server *server,
	const char *url, int port);

static inline const char *brubeck_sampler_name(struct brubeck_sampler *sampler)
{
	switch (sampler->type) {
		case BRUBECK_SAMPLER_STATSD: return "statsd";
		case BRUBECK_SAMPLER_STATSD_SECURE: return "statsd-secure";
		default: return NULL;
	}
}

#include "samplers/statsd.h"

#endif

