#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>

#define MAX_PACKET_SIZE 512
#define MAX_THREADS 4
#define MAX_FANOUT 4

#ifdef __gnu_linux__
#define LARGE_SOCK_SIZE 33554431
#else
#define LARGE_SOCK_SIZE 4096
#endif

static struct {
	unsigned int fanout;
	int in_socket;
	struct sockaddr_in in_addr;
	struct {
		int socket;
		struct sockaddr_in addr;
	} out[MAX_FANOUT];
	pthread_t threads[MAX_THREADS];
} _balancer;

static void diep(char *s)
{
	perror(s);
	exit(1);
}

int sock_enlarge_in(int fd)
{
	int bs = LARGE_SOCK_SIZE;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bs, sizeof(bs)) == -1)
		diep("setsockopt");

	return 0;
}

int sock_enlarge_out(int fd)
{
	int bs = LARGE_SOCK_SIZE;

	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bs, sizeof(bs)) == -1)
		diep("setsockopt");

	return 0;
}

#if 0
static void *balancer__worker(void *_)
{
	const socklen_t addrlen = sizeof(struct sockaddr_in);
	unsigned int i;

	struct iovec iovecs[SIMULTANEOUS_PACKETS];
	struct mmsghdr msgs[SIMULTANEOUS_PACKETS];

	memset(msgs, 0x0, sizeof(msgs));

	for (i = 0; i < SIMULTANEOUS_PACKETS; ++i) {
		iovecs[i].iov_base = xmalloc(MAX_PACKET_SIZE);
		iovecs[i].iov_len = MAX_PACKET_SIZE;
		msgs[i].msg_hdr.msg_iov = &iovecs[i];
		msgs[i].msg_hdr.msg_iovlen = 1;
	}

	for (;;) {
		int res = recvmmsg(_balancer.in_socket, msgs, SIMULTANEOUS_PACKETS, 0, NULL);

		if (res <= 0)
			continue;

		for (i = 0; i < SIMULTANEOUS_PACKETS; ++i) {
			const char *buf = msgs[i].msg_hdr.msg_iov->iov_base;
			size_t len = msgs[i].msg_len;

			for (j = 0; j < FANOUT; ++j)
				sendto(_balancer.out[j].socket, buf, len, 0, &_balancer.out[j].addr, addrlen);
		}
	}

	return NULL;
}
#endif

static void *balancer__worker(void *_)
{
	const socklen_t addrlen = sizeof(struct sockaddr_in);
	unsigned int j;

	char buffer[MAX_PACKET_SIZE];

	for (;;) {
		int res = recvfrom(_balancer.in_socket, buffer,
			sizeof(buffer), 0, NULL, NULL);

		if (res <= 0)
			continue;

		for (j = 0; j < _balancer.fanout; ++j)
			sendto(_balancer.out[j].socket, buffer, (size_t)res, 0,
				(struct sockaddr *)&_balancer.out[j].addr, addrlen);
	}

	return NULL;
}

static void init_sock(struct sockaddr_in *addr, int *sock, char *address_s)
{
	char *port_s = strchr(address_s, ':');
	int port;

	if (port_s == NULL) {
		fprintf(stderr, "Invalid address: %s\n", address_s);
		exit(1);
	}

	*port_s = '\0';
	port = atoi(port_s + 1);

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = (strcmp(address_s, "0.0.0.0")) ?
		inet_addr(address_s) : htonl(INADDR_ANY);

	*sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (*sock < 0)
		diep("socket");
}

static void usage(const char *progname)
{
	printf("Usage: %s [--listen addr] addr...\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int i = 1;

	if (argc > 1 && strcmp(argv[1], "--listen") == 0) {
		if (argc == 2)
			usage(argv[0]);

		init_sock(&_balancer.in_addr, &_balancer.in_socket, argv[2]);
		sock_enlarge_in(_balancer.in_socket);

		if (bind(_balancer.in_socket, (struct sockaddr *)&_balancer.in_addr,
			sizeof(_balancer.in_addr)) < 0)
			diep("bind");

		i = 3;
	}

	for (; i < argc; ++i) {
		if (_balancer.fanout == MAX_FANOUT)
			usage(argv[0]);

		init_sock(
			&_balancer.out[_balancer.fanout].addr,
			&_balancer.out[_balancer.fanout].socket,
			argv[i]);

		sock_enlarge_out(_balancer.out[_balancer.fanout].socket);
		_balancer.fanout++;
	}

	if (!_balancer.fanout) {
		fprintf(stderr, "No fanout addresses to proxy.\n");
		exit(1);
	}

	for (i = 0; i < MAX_THREADS; ++i)
		pthread_create(&_balancer.threads[i], NULL, &balancer__worker, NULL);

	for (i =0; i < MAX_THREADS; ++i)
		pthread_join(_balancer.threads[i], NULL);

	return 0;
}
