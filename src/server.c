#include <sys/signalfd.h>
#include <sys/signal.h>
#include <sys/timerfd.h>
#include <poll.h>

#include "brubeck.h"

#if JANSSON_VERSION_HEX < 0x020500
#	error "libjansson-dev is too old (2.5+ required)"
#endif

static void
update_flows(struct brubeck_server *server)
{
	int i;
	for (i = 0; i < server->active_samplers; ++i) {
		struct brubeck_sampler *sampler = server->samplers[i];
		sampler->current_flow = sampler->inflow;
		sampler->inflow = 0;
	}
}

#define UTF8_UPARROW "\xE2\x86\x91"
#define UTF8_DOWNARROW "\xE2\x86\x93"

static void
update_proctitle(struct brubeck_server *server)
{
	static const char *size_suffix[] = { "b", "kb", "mb", "gb", "tb", "pb", "eb" };
#define PUTS(...) pos += snprintf(buf + pos, sizeof(buf) - pos, __VA_ARGS__)
	char buf[2048];
	int i, j, pos = 0;

	PUTS("[%s] [ " UTF8_UPARROW, server->config_name);

	for (i = 0; i < server->active_backends; ++i) {
		struct brubeck_backend *backend = server->backends[i];
		if (backend->type == BRUBECK_BACKEND_CARBON) {
			struct brubeck_carbon *carbon = (struct brubeck_carbon *)backend;
			double sent = carbon->sent;

			for (j = 0; j < 7 && sent >= 1024.0; ++j)
				sent /= 1024.0;

			PUTS("%s #%d %.1f%s%s",
				(i > 0) ? "," : "",
				i + 1, sent, size_suffix[j],
				(carbon->out_sock >= 0) ? "" : " (dc)");
		}
	}

	PUTS(" ] [ " UTF8_DOWNARROW);

	for (i = 0; i < server->active_samplers; ++i) {
		struct brubeck_sampler *sampler = server->samplers[i];
		PUTS("%s :%d %d/s",
				(i > 0) ? "," : "",
				(int)ntohs(sampler->addr.sin_port),
				(int)sampler->current_flow);
	}

	PUTS(" ]");
	setproctitle("brubeck", buf);
}

static void
expire_metric(struct brubeck_metric *mt, void *_)
{
	/* If this metric is not disabled, turn "inactive"
	 * into "disabled" and "active" into "inactive"
	 */
	if (mt->expire > BRUBECK_EXPIRE_DISABLED)
		mt->expire = mt->expire - 1;
}

static void
dump_metric(struct brubeck_metric *mt, void *out_file)
{
	static const char *METRIC_NAMES[] = {"g", "c", "C", "h", "ms", "s", "internal"};
	fprintf((FILE *)out_file, "%s|%s\n", mt->key, METRIC_NAMES[mt->type]);
}

static void
dump_all_metrics(struct brubeck_server *server)
{
	FILE *dump = NULL;

	log_splunk("event=dump_metrics");

	if (server->dump_path)
		dump = fopen(server->dump_path, "w+");

	if (!dump) {
		log_splunk_errno("event=dump_failed");
		return;
	}

	brubeck_hashtable_foreach(server->metrics, &dump_metric, dump);
	fclose(dump);
}

static void load_backends(struct brubeck_server *server, json_t *backends)
{
	size_t idx;
	json_t *b;

	json_array_foreach(backends, idx, b) {
		const char *type = json_string_value(json_object_get(b, "type"));
		struct brubeck_backend *backend = NULL;

		if (type && !strcmp(type, "carbon")) {
			backend = brubeck_carbon_new(server, b, server->active_backends);
			server->backends[server->active_backends++] = backend; 
		} else {
			log_splunk("backend=%s event=invalid_backend", type);
		}
	}
}

static void load_samplers(struct brubeck_server *server, json_t *samplers)
{
	size_t idx;
	json_t *s;

	json_array_foreach(samplers, idx, s) {
		const char *type = json_string_value(json_object_get(s, "type"));

		if (type && !strcmp(type, "statsd")) {
			server->samplers[server->active_samplers++] = brubeck_statsd_new(server, s);
		} else if (type && !strcmp(type, "statsd-secure")) {
			server->samplers[server->active_samplers++] = brubeck_statsd_secure_new(server, s);
		} else {
			log_splunk("sampler=%s event=invalid_sampler", type);
		}
	}
}

static int load_timerfd(int interval)
{
	struct itimerspec timer;
	int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);

    if (timerfd < 0)
		die("failed to create timer");

	memset(&timer, 0x0, sizeof(timer));
	timer.it_value.tv_sec = interval;
	timer.it_value.tv_nsec = 0;
	timer.it_interval.tv_sec = interval;
	timer.it_interval.tv_nsec = 0;

	if (timerfd_settime(timerfd, 0, &timer, NULL) < 0)
		die("failed to set system timer");

	return timerfd;
}

static int load_signalfd(void)
{
	sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGUSR2);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		die("failed to sigprocmask the needed signals");

	return signalfd(-1, &mask, 0);
}

static char *get_config_name(const char *full_path)
{
	const char *filename = strrchr(full_path, '/');
	char *config_name = strdup(filename ? filename + 1 : full_path);
	char *ext = strrchr(config_name, '.');

	if (ext)
		*ext = '\0';

	return config_name;
}

static void load_config(struct brubeck_server *server, const char *path)
{
	json_error_t error;

	/* required */
	int capacity;
	json_t *backends, *samplers;

	/* optional */
	int expire = 0;
	char *http = NULL;

	server->name = "brubeck";
	server->config_name = get_config_name(path);
	server->dump_path = NULL;
	server->config = json_load_file(path, 0, &error);
	if (!server->config) {
		die("failed to load config file, %s (%s:%d:%d)",
				error.text, error.source, error.line, error.column);
	}

	json_unpack_or_die(server->config,
		"{s?:s, s:s, s:i, s:o, s:o, s?:s, s?:i}",
		"server_name", &server->name,
		"dumpfile", &server->dump_path,
		"capacity", &capacity,
		"backends", &backends,
		"samplers", &samplers,
		"http", &http,
		"expire", &expire);

	gh_log_set_instance(server->name);

	server->metrics = brubeck_hashtable_new(1 << capacity);
	if (!server->metrics)
	    die("failed to initialize hash table (size: %lu)", 1ul << capacity);

	load_backends(server, backends);
	load_samplers(server, samplers);

	if (http) brubeck_http_endpoint_init(server, http);
	if (expire) server->fd_expire = load_timerfd(expire);
}

void brubeck_server_init(struct brubeck_server *server, const char *config)
{
	memset(server, 0x0, sizeof(struct brubeck_server));

	/* ignore SIGPIPE here so we don't crash when
	 * backends get disconnected */
	signal(SIGPIPE, SIG_IGN);

	server->fd_signal = load_signalfd();
	server->fd_update = load_timerfd(1);
	server->fd_expire = -1;

	/* init the memory allocator */
	brubeck_slab_init(&server->slab);

	/* init the samplers and backends */
	load_config(server, config);

	/* Init the internal stats */
	brubeck_internal__init(server);
}

static int timer_elapsed(struct pollfd *fd)
{
	if (fd->revents & POLLIN) {
		uint64_t timer;
		int s = read(fd->fd, &timer, sizeof(timer));
		return (s == sizeof(timer));
	}
	return 0;
}

static int signal_triggered(struct pollfd *fd)
{
	if (fd->revents & POLLIN) {
		struct signalfd_siginfo fdsi;
		int s = read(fd->fd, &fdsi, sizeof(fdsi));
		if (s == sizeof(fdsi))
			return fdsi.ssi_signo;
	}
	return -1;
}

int brubeck_server_run(struct brubeck_server *server)
{
	struct pollfd fds[3];
	int nfd = 2;
	size_t i;

	memset(fds, 0x0, sizeof(fds));

	fds[0].fd = server->fd_signal;
	fds[0].events = POLLIN;

	fds[1].fd = server->fd_update;
	fds[1].events = POLLIN;

	if (server->fd_expire >= 0) {
		fds[2].fd = server->fd_expire;
		fds[2].events = POLLIN;
		nfd++;
	}

	server->running = 1;
	log_splunk("event=listening");

	while (server->running) {
		if (poll(fds, nfd, -1) < 0)
			continue;

		switch (signal_triggered(&fds[0])) {
		case SIGHUP:
			gh_log_reopen();
			log_splunk("event=reload_log");
			break;
		case SIGUSR2:
			dump_all_metrics(server);
			break;
		case SIGINT:
		case SIGTERM:
			server->running = 0;
			break;
		}

		if (timer_elapsed(&fds[1])) {
			update_flows(server);
			update_proctitle(server);
		}

		if (timer_elapsed(&fds[2])) {
			log_splunk("event=expire_metrics");
			brubeck_hashtable_foreach(server->metrics, &expire_metric, NULL);
		}
	}

	for (i = 0; i < server->active_backends; ++i)
		pthread_cancel(server->backends[i]->thread);

	for (i = 0; i < server->active_samplers; ++i) {
		struct brubeck_sampler *sampler = server->samplers[i];
		if (sampler->shutdown)
			sampler->shutdown(sampler);
	}

	log_splunk("event=shutdown");
	return 0;
}

