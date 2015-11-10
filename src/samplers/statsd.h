#ifndef __BRUBECK_STATSD_H__
#define __BRUBECK_STATSD_H__

#include "bloom.h"

struct brubeck_statsd_msg {
	char *key;      /* The key of the message, NULL terminated */
	uint16_t key_len; /* length of the key */
	uint16_t type;	/* type of the messaged, as a brubeck_mt_t */
	value_t value;	/* floating point value of the message */
	value_t sample_freq; /* floating poit sample freq (1.0 / sample_rate) */
	uint8_t modifiers; /* modifiers, as a brubeck_metric_mod_t */
};

struct brubeck_statsd {
	struct brubeck_sampler sampler;
	pthread_t *workers;
	unsigned int worker_count;
	unsigned int mmsg_count;
};

struct brubeck_statsd_secure {
	struct brubeck_sampler sampler;
	const char *hmac_key;

	struct multibloom *replays;
	time_t now;
	time_t drift;

	pthread_t thread;
};

void brubeck_statsd_packet_parse(struct brubeck_server *server, char *buffer, char *end);
int brubeck_statsd_msg_parse(struct brubeck_statsd_msg *msg, char *buffer, char *end);

struct brubeck_sampler * brubeck_statsd_secure_new(struct brubeck_server *server, json_t *settings);
struct brubeck_sampler *brubeck_statsd_new(struct brubeck_server *server, json_t *settings);

#endif
