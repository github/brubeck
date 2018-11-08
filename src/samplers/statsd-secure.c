#include <stddef.h>
#include <time.h>
#include <openssl/hmac.h>
#include "brubeck.h"

#define SHA_SIZE 32
#define SHA_FUNCTION EVP_sha256

#define MAX_PACKET_SIZE 1024
#define MIN_PACKET_SIZE (SHA_SIZE + 12)

static int
memcmpct(const void *_a, const void *_b, size_t len)
{
	const unsigned char *a = _a;
	const unsigned char *b = _b;
	size_t i;
	int cmp = 0;

	for (i = 0; i < len; ++i)
		cmp |= a[i] ^ b[i];

	return cmp;
}

static const char *
hmactos(const char *buffer)
{
	static const char hex_str[] = "0123456789abcdef";
	static __thread char hex_hmac[SHA_SIZE * 2 + 1];

	unsigned int i, j;

	for (i = 0, j = 0; i < SHA_SIZE; i++) {
		hex_hmac[j++] = hex_str[buffer[i] >> 4];
		hex_hmac[j++] = hex_str[buffer[i] & 0xF];
	}

	hex_hmac[j] = 0;

	return hex_hmac;
}

static int
verify_token(struct brubeck_server *server, struct brubeck_statsd_secure *statsd, const char *buffer)
{
	uint32_t ha, hb;
	uint64_t timestamp;
	struct timespec now;

	memcpy(&timestamp, buffer + SHA_SIZE, 8);
	clock_gettime(CLOCK_REALTIME, &now);

	if (now.tv_sec != statsd->now) {
		statsd->now = now.tv_sec;
		multibloom_reset(statsd->replays, statsd->now % statsd->drift);
	}

	/* token from the future? */
	if (statsd->now < timestamp) {
		log_splunk(
				"sampler=statsd-secure event=fail_future now=%llu timestamp=%llu",
				(long long unsigned int)statsd->now,
				(long long unsigned int)timestamp
		);
		brubeck_stats_inc(server, secure.from_future);
		return -1;
	}

	/* delayed */
	if (statsd->now - timestamp > statsd->drift) {
		log_splunk(
				"sampler=statsd-secure event=fail_delayed now=%llu timestamp=%llu drift=%d",
				(long long unsigned int)statsd->now,
				(long long unsigned int)timestamp,
				(int)(statsd->now - timestamp)
		);
		brubeck_stats_inc(server, secure.delayed);
		return -1;
	}

	memcpy(&ha, buffer, sizeof(ha));
	memcpy(&hb, buffer + 4, sizeof(hb));

	if (multibloom_check(statsd->replays, timestamp % statsd->drift, ha, hb)) {
		log_splunk("sampler=statsd-secure event=fail_replayed hmac=%s", hmactos(buffer));
		brubeck_stats_inc(server, secure.replayed);
		return -1;
	}

	return 0;
}

static void *statsd_secure__thread(void *_in)
{
	struct brubeck_statsd_secure *statsd = _in;
	struct brubeck_server *server = statsd->sampler.server;

	char buffer[MAX_PACKET_SIZE];

	HMAC_CTX ctx;
	unsigned char hmac_buffer[SHA_SIZE];
	unsigned int hmac_len;

	struct sockaddr_in reporter;
	socklen_t reporter_len = sizeof(reporter);
	memset(&reporter, 0, reporter_len);

	log_splunk("sampler=statsd-secure event=worker_online");

	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, statsd->hmac_key, strlen(statsd->hmac_key), SHA_FUNCTION(), NULL);

	for (;;) {
		int res = recvfrom(statsd->sampler.in_sock, buffer,
			sizeof(buffer) - 1, 0, (struct sockaddr *)&reporter, &reporter_len);

		if (res < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			log_splunk_errno("sampler=statsd-secure event=failed_read from=%s",
				inet_ntoa(reporter.sin_addr));
			brubeck_stats_inc(server, errors);
			continue;
		}

		brubeck_atomic_inc(&statsd->sampler.inflow);

		if (res < MIN_PACKET_SIZE) {
			log_splunk("sampler=statsd-secure event=short_pkt len=%d", res);
			brubeck_stats_inc(server, secure.failed);
			continue;
		}

		HMAC_Init_ex(&ctx, NULL, 0, NULL, NULL);
		HMAC_Update(&ctx, (unsigned char *)buffer + SHA_SIZE, res - SHA_SIZE);
		HMAC_Final(&ctx, hmac_buffer, &hmac_len);

		if (memcmpct(buffer, hmac_buffer, SHA_SIZE) != 0) {
			log_splunk("sampler=statsd-secure event=fail_auth hmac=%s", hmactos(buffer));
			brubeck_stats_inc(server, secure.failed);
			continue;
		}

		if (verify_token(server, statsd, buffer) < 0)
			continue;

		// TODO FIX brubeck_statsd_packet_parse(server, buffer + MIN_PACKET_SIZE, buffer + res, statsd->key_prefix);
	}

	HMAC_CTX_cleanup(&ctx);
	return NULL;
}

static void shutdown_sampler(struct brubeck_sampler *sampler)
{
	struct brubeck_statsd_secure *statsd = (struct brubeck_statsd_secure *)sampler;
	pthread_cancel(statsd->thread);
}

struct brubeck_sampler *
brubeck_statsd_secure_new(struct brubeck_server *server, json_t *settings)
{
	struct brubeck_statsd_secure *std = xmalloc(sizeof(struct brubeck_statsd_secure));
	char *address;
	int port, replay_len, drift;

	std->sampler.shutdown = &shutdown_sampler;
	std->sampler.type = BRUBECK_SAMPLER_STATSD_SECURE;
	std->now = 0;

	json_unpack_or_die(settings,
		"{s:s, s:i, s:s, s:i, s:i}",
		"address", &address,
		"port", &port,
		"hmac_key", &std->hmac_key,
		"max_drift", &drift,
		"replay_len", &replay_len);

	brubeck_sampler_init_inet((struct brubeck_sampler *)std, server, address, port);
	std->drift = (time_t)drift;
	std->replays = multibloom_new(std->drift, replay_len, 0.001);
	std->sampler.in_sock = brubeck_sampler_socket(&std->sampler, 0);

	if (pthread_create(&std->thread, NULL, &statsd_secure__thread, std) != 0)
		die("failed to start sampler thread");

	return (struct brubeck_sampler *)std;
}
