#include "brubeck.h"

void
brubeck_sampler_init_inet(struct brubeck_sampler *sampler, struct brubeck_server *server, const char *url, int port)
{
	sampler->server = server;
	url_to_inaddr2(&sampler->addr, url, port);

	log_splunk("sampler=%s event=load_udp addr=0.0.0.0:%d",
		brubeck_sampler_name(sampler), port);
}

int brubeck_sampler_socket(struct brubeck_sampler *sampler, int multisock)
{
	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	assert(sock >= 0);

	sock_enlarge_in(sock);
	sock_setreuse(sock, 1);
	
	if (multisock)
		sock_setreuse_port(sock, 1);

	if (bind(sock, (struct sockaddr *)&sampler->addr, sizeof(sampler->addr)) < 0)
		die("failed to bind socket");

	return sock;
}
