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

#define MAX_PACKET_SIZE 512

#ifdef HAVE_RECVMMSG
static void statsd_run_recvmmsg(struct brubeck_statsd *statsd, int sock)
{
	const unsigned int SIM_PACKETS = statsd->mmsg_count;
	struct brubeck_server *server = statsd->sampler.server;

	struct brubeck_statsd_msg msg;
	struct brubeck_metric *metric;
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
		int res = recvmmsg(sock, msgs, SIM_PACKETS, 0, NULL);

		if (res < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			log_splunk_errno("sampler=statsd event=failed_read");
			brubeck_server_mark_dropped(server);
			continue;
		}

		/* store stats */
		brubeck_atomic_add(&server->stats.metrics, SIM_PACKETS);
		brubeck_atomic_add(&statsd->sampler.inflow, SIM_PACKETS);

		for (i = 0; i < SIM_PACKETS; ++i) {
			char *buf = msgs[i].msg_hdr.msg_iov->iov_base;
			int len = msgs[i].msg_len;

			if (brubeck_statsd_msg_parse(&msg, buf, len) < 0) {
				if (msg.key_len > 0)
					buf[msg.key_len] = ':';

				log_splunk("sampler=statsd event=bad_key key='%.*s'", len, buf);

				brubeck_server_mark_dropped(server);
				continue;
			}

			metric = brubeck_metric_find(server, msg.key, msg.key_len, msg.type);
			if (metric != NULL)
				brubeck_metric_record(metric, msg.value);
		}
	}
}
#endif

static void statsd_run_recvmsg(struct brubeck_statsd *statsd, int sock)
{
	struct brubeck_server *server = statsd->sampler.server;

	struct brubeck_statsd_msg msg;
	struct brubeck_metric *metric;

	char buffer[MAX_PACKET_SIZE];

	struct sockaddr_in reporter;
	socklen_t reporter_len = sizeof(reporter);
	memset(&reporter, 0, reporter_len);

	log_splunk("sampler=statsd event=worker_online syscall=recvmsg socket=%d", sock);

	for (;;) {
		int res = recvfrom(sock, buffer,
			sizeof(buffer) - 1, 0,
			(struct sockaddr *)&reporter, &reporter_len);

		if (res < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			log_splunk_errno("sampler=statsd event=failed_read from=%s",
				inet_ntoa(reporter.sin_addr));
			brubeck_server_mark_dropped(server);
			continue;
		}

		/* store stats */
		brubeck_atomic_inc(&server->stats.metrics);
		brubeck_atomic_inc(&statsd->sampler.inflow);

		if (brubeck_statsd_msg_parse(&msg, buffer, (size_t)res) < 0) {
			if (msg.key_len > 0)
				buffer[msg.key_len] = ':';

			log_splunk("sampler=statsd event=bad_key key='%.*s' from=%s",
				res, buffer, inet_ntoa(reporter.sin_addr));

			brubeck_server_mark_dropped(server);
			continue;
		}

		metric = brubeck_metric_find(server, msg.key, msg.key_len, msg.type);
		if (metric != NULL) {
			brubeck_metric_record(metric, msg.value);
		}
	}

}

int brubeck_statsd_msg_parse(struct brubeck_statsd_msg *msg, char *buffer, size_t length)
{
	char *end = buffer + length;
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
		int negative = 0;
		char *start = buffer;

		msg->value = 0.0;

		if (*buffer == '-') {
			++buffer;
			negative = 1;
		}

		while (*buffer >= '0' && *buffer <= '9') {
			msg->value = (msg->value * 10.0) + (*buffer - '0');
			++buffer;
		}

		if (*buffer == '.') {
			double f = 0.0, n = 0.0;
			++buffer;

			while (*buffer >= '0' && *buffer <= '9') {
				f = (f * 10.0) + (*buffer - '0');
				++buffer;
				n += 1.0;
			}

			msg->value += f / pow(10.0, n);
		}

		if (negative)
			msg->value = -msg->value;

		if (unlikely(*buffer == 'e')) {
			msg->value = strtod(start, &buffer);
		}

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
	}

	/**
	 * Trailing bytes: data appended at the end of the message.
	 * This is stored verbatim and will be parsed when processing
	 * the specific message type. This is optional.
	 *
	 *      gorets:1|c|@0.1
	 *                 ^^^^----
	 */
	{
		buffer++;

		if (buffer[0] == '\0' || (buffer[0] == '\n' && buffer[1] == '\0')) {
			msg->trail = NULL;
			return 0;
		}
			
		if (*buffer == '@' || *buffer == '|') {
			msg->trail = buffer;
			return 0;
		}

		return -1;
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
