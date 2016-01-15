#include <stddef.h>
#include <string.h>
#include "brubeck.h"

static const char carbon_empty_str[] = "";
static const char carbon_global_prefix[] = "stats.";
static const char carbon_global_count_prefix[] = "stats_counts.";
static const char carbon_prefix_counter[] = "counters.";
static const char carbon_prefix_timer[] = "timers.";
static const char carbon_prefix_gauge[] = "gauges.";

static inline int is_connected(struct brubeck_carbon *self)
{
	return (self->out_sock >= 0);
}

static int carbon_connect(void *backend)
{
	struct brubeck_carbon *self = (struct brubeck_carbon *)backend;

	if (is_connected(self))
		return 0;

	self->out_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (self->out_sock >= 0) {
		int rc = connect(self->out_sock,
				(struct sockaddr *)&self->out_sockaddr,
				sizeof(self->out_sockaddr));
		
		if (rc == 0) {
			log_splunk("backend=carbon event=connected");
			sock_enlarge_out(self->out_sock);
			return 0;
		}

		close(self->out_sock);
		self->out_sock = -1;
	}

	log_splunk_errno("backend=carbon event=failed_to_connect");
	return -1;
}

static void carbon_disconnect(struct brubeck_carbon *self)
{
	log_splunk_errno("backend=carbon event=disconnected");

	close(self->out_sock);
	self->out_sock = -1;
}

static int carbon_namespace(
	char *out_key,
	const struct brubeck_metric *metric,
	const char *key,
	const uint8_t key_len,
	const struct brubeck_carbon *carbon,
	uint8_t counter_abs)
{
	char *ptr = out_key;

	uint8_t prefix_len;
	if (!carbon->legacy_namespace ||
		!(IS_COUNTER(metric->type) && counter_abs)) {

		prefix_len = strlen(carbon->global_prefix);
		memcpy(ptr, carbon->global_prefix, prefix_len);
		ptr += prefix_len;
	}
	else {
		prefix_len = strlen(carbon->global_count_prefix);
		memcpy(ptr, carbon->global_count_prefix, prefix_len);
		ptr += prefix_len;
	}

	uint8_t metric_prefix_len = 0;
	switch (metric->type) {
		case BRUBECK_MT_COUNTER:
		case BRUBECK_MT_METER:
			metric_prefix_len = strlen(carbon->prefix_counter);
			memcpy(ptr, carbon->prefix_counter, metric_prefix_len);
			break;
		case BRUBECK_MT_TIMER:
			metric_prefix_len = strlen(carbon->prefix_timer);
			memcpy(ptr, carbon->prefix_timer, metric_prefix_len);
			break;
		case BRUBECK_MT_GAUGE:
			metric_prefix_len = strlen(carbon->prefix_gauge);
			memcpy(ptr, carbon->prefix_gauge, metric_prefix_len);
			break;
		default:
			break;
	}

	ptr += metric_prefix_len;

	memcpy(ptr, key, key_len);
	ptr += key_len;

	if (IS_COUNTER(metric->type) && !carbon->legacy_namespace) {
		if (counter_abs) {
			memcpy(ptr, ".count", strlen(".count"));
			ptr += strlen(".count");
		}
		else {
			memcpy(ptr, ".rate", strlen(".rate"));
			ptr += strlen(".rate");
		}
	}

	return ptr - out_key;
}

static void plaintext_send(
	const char *key,
	uint8_t key_len,
	value_t value,
	void *backend)
{
	struct brubeck_carbon *carbon = (struct brubeck_carbon *)backend;
	char buffer[1024];
	char *ptr = buffer;
	ssize_t wr;

	if (!is_connected(carbon))
		return;

	memcpy(ptr, key, key_len);
	ptr += key_len;
	*ptr++ = ' ';

	ptr += brubeck_ftoa(ptr, value);
	*ptr++ = ' ';

	ptr += brubeck_itoa(ptr, carbon->backend.tick_time);
	*ptr++ = '\n';

	wr = write_in_full(carbon->out_sock, buffer, ptr - buffer);
	if (wr < 0) {
		carbon_disconnect(carbon);
		return;
	}

	carbon->sent += wr;
}

static void plaintext_each(
	const struct brubeck_metric *metric,
	const char *key,
	value_t value,
	void *backend)
{
	struct brubeck_carbon *carbon = (struct brubeck_carbon *)backend;
	size_t key_len = strlen(key);

	if (!carbon->namespacing || metric->type == BRUBECK_MT_INTERNAL_STATS) {
		plaintext_send(key, key_len, value, backend);
		return;
	}

	char prefix_key[1024];
	uint8_t prefix_key_len = 0;

	prefix_key_len = carbon_namespace(prefix_key, metric, key, key_len, carbon, true);
	plaintext_send(prefix_key, prefix_key_len, value, backend);

	if (IS_COUNTER(metric->type) &&
		carbon->backend.sample_freq != 0) {
		prefix_key_len = carbon_namespace(prefix_key, metric,
			key, key_len, carbon, false);
		value_t normalized_val = value / carbon->backend.sample_freq;
		plaintext_send(prefix_key, prefix_key_len, normalized_val, backend);
	}
}

static inline size_t pickle1_int32(char *ptr, void *_src)
{
	*ptr = 'J';
	memcpy(ptr + 1, _src, 4);
	return 5;
}

static inline size_t pickle1_double(char *ptr, void *_src)
{
	uint8_t *source = _src;

	*ptr++ = 'G';

	ptr[0] = source[7];
	ptr[1] = source[6];
	ptr[2] = source[5];
	ptr[3] = source[4];
	ptr[4] = source[3];
	ptr[5] = source[2];
	ptr[6] = source[1];
	ptr[7] = source[0];

	return 9;
}

static void pickle1_push(
		struct pickler *buf,
		const char *key,
		uint8_t key_len,
		uint32_t timestamp,
		value_t value)
{
	char *ptr = buf->ptr + buf->pos;

	*ptr++ = '(';

	*ptr++ = 'U';
	*ptr++ = key_len;
	memcpy(ptr, key, key_len);
	ptr += key_len;

	*ptr++ = 'q';
	*ptr++ = buf->pt++;

	*ptr++ = '(';

	ptr += pickle1_int32(ptr, &timestamp);
	ptr += pickle1_double(ptr, &value);

	*ptr++ = 't';
	*ptr++ = 'q';
	*ptr++ = buf->pt++;

	*ptr++ = 't';
	*ptr++ = 'q';
	*ptr++ = buf->pt++;

	buf->pos = (ptr - buf->ptr);
}

static inline void pickle1_init(struct pickler *buf)
{
	static const uint8_t lead[] = { ']', 'q', 0, '(' };

	memcpy(buf->ptr + 4, lead, sizeof(lead));
	buf->pos = 4 + sizeof(lead);
	buf->pt = 1;
}

static void pickle1_flush(void *backend)
{
	static const uint8_t trail[] = {'e', '.'};

	struct brubeck_carbon *carbon = (struct brubeck_carbon *)backend;
	struct pickler *buf = &carbon->pickler;

	uint32_t *buf_lead;
	ssize_t wr;
	
	if (buf->pt == 1 || !is_connected(carbon))
		return;

	memcpy(buf->ptr + buf->pos, trail, sizeof(trail));
	buf->pos += sizeof(trail);

	buf_lead = (uint32_t *)buf->ptr;
	*buf_lead = htonl((uint32_t)buf->pos - 4);

	wr = write_in_full(carbon->out_sock, buf->ptr, buf->pos);

	pickle1_init(&carbon->pickler);
	if (wr < 0) {
		carbon_disconnect(carbon);
		return;
	}

	carbon->sent += wr;
}

static void pickle1_each(
	const struct brubeck_metric *metric,
	const char *key,
	value_t value,
	void *backend)
{
	struct brubeck_carbon *carbon = (struct brubeck_carbon *)backend;
	uint8_t key_len = (uint8_t)strlen(key);

	if (carbon->pickler.pos + PICKLE1_SIZE(key_len)
		>= PICKLE_BUFFER_SIZE) {
		pickle1_flush(carbon);
	}

	if (!is_connected(carbon))
		return;

	if (!carbon->namespacing || metric->type == BRUBECK_MT_INTERNAL_STATS) {
		pickle1_push(&carbon->pickler, key, key_len,
			carbon->backend.tick_time, value);
		return;
	}

	char prefix_key[1024];
	uint8_t prefix_key_len = 0;

	prefix_key_len = carbon_namespace(prefix_key, metric, key, key_len, carbon, true);
	pickle1_push(&carbon->pickler, prefix_key, prefix_key_len,
		carbon->backend.tick_time, value);

	if (IS_COUNTER(metric->type) &&
		carbon->backend.sample_freq != 0) {
		prefix_key_len = carbon_namespace(prefix_key, metric,
			key, key_len, carbon, false);
		value_t normalized_val = value / carbon->backend.sample_freq;
		pickle1_push(&carbon->pickler, prefix_key, prefix_key_len,
			carbon->backend.tick_time, normalized_val);
	}
}

struct brubeck_backend *
brubeck_carbon_new(struct brubeck_server *server, json_t *settings, int shard_n)
{
	struct brubeck_carbon *carbon = xcalloc(1, sizeof(struct brubeck_carbon));
	char *address;
	const char *global_prefix = carbon_global_prefix;
	const char *global_count_prefix = carbon_global_count_prefix;
	const char *prefix_counter = carbon_prefix_counter;
	const char *prefix_timer = carbon_prefix_timer;
	const char *prefix_gauge = carbon_prefix_gauge;
	int port, frequency, pickle, namespacing = 0;
	int legacy_namespace = 1;

	json_unpack_or_die(settings,
		"{s:s, s:i, s?:b, s:i, s?:b, s?:b, s?:s, s?:s, s?:s, s?:s, s?:s}",
		"address", &address,
		"port", &port,
		"pickle", &pickle,
		"frequency", &frequency,

		"namespacing", &namespacing,
		"legacy_namespace", &legacy_namespace,
		"global_prefix", &global_prefix,
		"global_count_prefix", &global_count_prefix,
		"prefix_counter", &prefix_counter,
		"prefix_timer", &prefix_timer,
		"prefix_gauge", &prefix_gauge);

	carbon->backend.type = BRUBECK_BACKEND_CARBON;
	carbon->backend.shard_n = shard_n;
	carbon->backend.connect = &carbon_connect;

	if (pickle) {
		carbon->backend.sample = &pickle1_each;
		carbon->backend.flush = &pickle1_flush;
		carbon->pickler.ptr = malloc(PICKLE_BUFFER_SIZE);
		pickle1_init(&carbon->pickler);
	} else {
		carbon->backend.sample = &plaintext_each;
		carbon->backend.flush = NULL;
	}

	carbon->backend.sample_freq = frequency;
	carbon->namespacing = namespacing;
	carbon->legacy_namespace = legacy_namespace;
	carbon->global_prefix = global_prefix;
	carbon->global_count_prefix = global_count_prefix;
	carbon->prefix_counter = prefix_counter;
	carbon->prefix_timer = prefix_timer;
	carbon->prefix_gauge = prefix_gauge;

	carbon->backend.server = server;
	carbon->out_sock = -1;
	url_to_inaddr2(&carbon->out_sockaddr, address, port);

	brubeck_backend_run_threaded((struct brubeck_backend *)carbon);
	log_splunk("backend=carbon event=started");

	return (struct brubeck_backend *)carbon;
}
