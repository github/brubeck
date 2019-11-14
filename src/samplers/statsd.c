#include <stddef.h>
#define _GNU_SOURCE
#include <sys/uio.h>
#include <sys/socket.h>
#include "brubeck.h"

#ifdef __GLIBC__
#	if ((__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 12)))
#		define HAVE_RECVMMSG 1
#	endif
#endif

#define MAX_PACKET_SIZE 8192

#ifdef HAVE_RECVMMSG

#ifndef MSG_WAITFORONE
#	define MSG_WAITFORONE 0x0
#endif

static void statsd_run_recvmmsg(struct brubeck_statsd *statsd, int sock)
{
	const unsigned int SIM_PACKETS = statsd->mmsg_count;
	struct brubeck_server *server = statsd->sampler.server;

	unsigned int i;
	struct iovec iovecs[SIM_PACKETS];
	struct mmsghdr msgs[SIM_PACKETS];

	memset(msgs, 0x0, sizeof(msgs));

	for (i = 0; i < SIM_PACKETS; ++i) {
		iovecs[i].iov_base = xmalloc(MAX_PACKET_SIZE);
		iovecs[i].iov_len = MAX_PACKET_SIZE - 1;
		msgs[i].msg_hdr.msg_iov = &iovecs[i];
		msgs[i].msg_hdr.msg_iovlen = 1;
	}

	log_splunk("sampler=statsd event=worker_online syscall=recvmmsg socket=%d", sock);

	for (;;) {
		int res = recvmmsg(sock, msgs, SIM_PACKETS, MSG_WAITFORONE, NULL);

		if (res < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			log_splunk_errno("sampler=statsd event=failed_read");
			brubeck_stats_inc(server, errors);
			continue;
		}

		/* store stats */
		brubeck_atomic_add(&statsd->sampler.inflow, res);

		for (i = 0; i < res; ++i) {
			char *buf = msgs[i].msg_hdr.msg_iov->iov_base;
			char *end = buf + msgs[i].msg_len;
			brubeck_statsd_packet_parse(server, buf, end);
		}
	}
}
#endif

static void statsd_run_recvmsg(struct brubeck_statsd *statsd, int sock)
{
	struct brubeck_server *server = statsd->sampler.server;

	char *buffer = xmalloc(MAX_PACKET_SIZE);
	struct sockaddr_in reporter;
	socklen_t reporter_len = sizeof(reporter);
	memset(&reporter, 0, reporter_len);

	log_splunk("sampler=statsd event=worker_online syscall=recvmsg socket=%d", sock);

	for (;;) {
		int res = recvfrom(sock, buffer, MAX_PACKET_SIZE - 1, 0,
			(struct sockaddr *)&reporter, &reporter_len);

		if (res < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			log_splunk_errno("sampler=statsd event=failed_read from=%s",
				inet_ntoa(reporter.sin_addr));
			brubeck_stats_inc(server, errors);
			continue;
		}

		brubeck_atomic_inc(&statsd->sampler.inflow);
		brubeck_statsd_packet_parse(server, buffer, buffer + res);
	}
}

static inline char *
parse_float(char *buffer, value_t *result, uint8_t *mods)
{
	int negative = 0;
	char *start = buffer;
	value_t value = 0.0;

	if (*buffer == '-') {
		++buffer;
		negative = 1;
		*mods |= BRUBECK_MOD_RELATIVE_VALUE;
	} else if (*buffer == '+') {
		++buffer;
		*mods |= BRUBECK_MOD_RELATIVE_VALUE;
	}

	while (*buffer >= '0' && *buffer <= '9') {
		value = (value * 10.0) + (*buffer - '0');
		++buffer;
	}

	if (*buffer == '.') {
		double f = 0.0;
		int n = 0;
		++buffer;

		while (*buffer >= '0' && *buffer <= '9') {
			f = (f * 10.0) + (*buffer - '0');
			buffer++;
			n++;
		}

		value += f / pow(10.0, n);
	}

	if (negative)
		value = -value;

	if (unlikely(*buffer == 'e' || *buffer == 'E'))
		value = strtod(start, &buffer);

	*result = value;
	return buffer;
}

int brubeck_statsd_msg_parse(struct brubeck_statsd_msg *msg, char *buffer, char *end)
{
	*end = '\0';

	/**
	 * Message key: all the string until the first ':'
	 *
	 *      gaugor:333|g
	 *      ^^^^^^
	 */
	{
		msg->key = buffer;
		msg->key_len = 0;
		while (*buffer != ':' && *buffer != '\0') {
			/* Invalid metric, can't have a space */
			if (*buffer == ' ')
				return -1;
			++buffer;
		}
		if (*buffer == '\0')
			return -1;

		msg->key_len = buffer - msg->key;
		*buffer++ = '\0';

		/* Corrupted metric. Graphite won't swallow this */
		if (msg->key[msg->key_len - 1] == '.')
			return -1;
	}

	/**
	 * Message value: the numeric value between ':' and '|'.
	 * This is already converted to an integer.
	 *
	 *      gaugor:333|g
	 *             ^^^
	 */
	{
		msg->modifiers = 0;
		buffer = parse_float(buffer, &msg->value, &msg->modifiers);

		if (*buffer != '|')
			return -1;

		buffer++;
	}

	/**
	 * Message type: one or two char identifier with the
	 * message type. Valid values: g, c, C, h, ms
	 *
	 *      gaugor:333|g
	 *                 ^
	 */
	{
		switch (*buffer) {
			case 'g': msg->type = BRUBECK_MT_GAUGE; break;
			case 'c': msg->type = BRUBECK_MT_METER; break;
			case 'C': msg->type = BRUBECK_MT_COUNTER; break;
			case 'h': msg->type = BRUBECK_MT_HISTO; break;
			case 'm':
					  ++buffer;
					  if (*buffer == 's') {
						  msg->type = BRUBECK_MT_TIMER;
						  break;
					  }

			default:
					  return -1;
		}

		buffer++;
	}

	/**
	 * Sample rate: parse the sample rate trailer if it exists.
	 * It must be a floating point number between 0.0 and 1.0
	 *
	 *      gorets:1|c|@0.1
	 *                 ^^^^----
	 */
	{
		if (buffer[0] == '|' && buffer[1] == '@') {
			double sample_rate;
			uint8_t dummy;

			buffer = parse_float(buffer + 2, &sample_rate, &dummy);
			if (sample_rate <= 0.0 || sample_rate > 1.0)
				return -1;

			msg->sample_freq = (1.0 / sample_rate);
		} else {
			msg->sample_freq = 1.0;
		}


		if (buffer[0] == '\0' || (buffer[0] == '\n' && buffer[1] == '\0'))
			return 0;
			
		return -1;
	}
}

void brubeck_statsd_packet_parse(struct brubeck_server *server, char *buffer, char *end)
{
	struct brubeck_statsd_msg msg;
	struct brubeck_metric *metric;

	while (buffer < end) {
		char *stat_end = memchr(buffer, '\n', end - buffer);
		if (!stat_end)
			stat_end = end;

		if (brubeck_statsd_msg_parse(&msg, buffer, stat_end) < 0) {
			brubeck_stats_inc(server, errors);
			log_splunk("sampler=statsd event=packet_drop");
		} else {
			brubeck_stats_inc(server, metrics);
			metric = brubeck_metric_find(server, msg.key, msg.key_len, msg.type);
			if (metric != NULL)
				brubeck_metric_record(metric, msg.value, msg.sample_freq, msg.modifiers);
		}

		/* move buf past this stat */
		buffer = stat_end + 1;
	}
}

static void *statsd__thread(void *_in)
{
	struct brubeck_statsd *statsd = _in;
	int sock = statsd->sampler.in_sock;

#ifdef SO_REUSEPORT
	if (sock < 0) {
		sock = brubeck_sampler_socket(&statsd->sampler, 1);
	}
#endif

	assert(sock >= 0);

#ifdef HAVE_RECVMMSG
	if (statsd->mmsg_count > 1) {
		statsd_run_recvmmsg(statsd, sock);
		return NULL;
	}
#endif

	statsd_run_recvmsg(statsd, sock);
	return NULL;
}

static void run_worker_threads(struct brubeck_statsd *statsd)
{
	unsigned int i;
	statsd->workers = xmalloc(statsd->worker_count * sizeof(pthread_t));

	for (i = 0; i < statsd->worker_count; ++i) {
		if (pthread_create(&statsd->workers[i], NULL, &statsd__thread, statsd) != 0)
			die("failed to start sampler thread");
	}
}

static void shutdown_sampler(struct brubeck_sampler *sampler)
{
	struct brubeck_statsd *statsd = (struct brubeck_statsd *)sampler;
	size_t i;

	for (i = 0; i < statsd->worker_count; ++i) {
		pthread_cancel(statsd->workers[i]);
	}
}

struct brubeck_sampler *
brubeck_statsd_new(struct brubeck_server *server, json_t *settings)
{
	struct brubeck_statsd *std = xmalloc(sizeof(struct brubeck_statsd));

	char *address;
	int port;
	int multisock = 0;

	std->sampler.type = BRUBECK_SAMPLER_STATSD;
	std->sampler.shutdown = &shutdown_sampler;
	std->sampler.in_sock = -1;
	std->worker_count = 4;
	std->mmsg_count = 1;

	json_unpack_or_die(settings,
		"{s:s, s:i, s?:i, s?:i, s?:b}",
		"address", &address,
		"port", &port,
		"workers", &std->worker_count,
		"multimsg", &std->mmsg_count,
		"multisock", &multisock);

	brubeck_sampler_init_inet(&std->sampler, server, address, port);

#ifndef SO_REUSEPORT
	multisock = 0;
#endif

	if (!multisock)
		std->sampler.in_sock = brubeck_sampler_socket(&std->sampler, 0);

	run_worker_threads(std);
	return &std->sampler;
}
