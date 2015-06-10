#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

void diep(char *s)
{
	perror(s);
	exit(1);
}

#define MAX_THREADS 4
#define SERVER_IP "127.0.0.1"
#define PORT 8126

static uint32_t counter; 

static int build_packet(char *buffer)
{
	static const char types[] = {'g', 'c', 'C', 'h'};
	int stat = rand() % 128;
	return sprintf(buffer, "github.test.packet.%d:%d|%c", stat, rand() % 1024, types[(stat * 0x37) % 4]);
}

static void *report_thread(void *_)
{
	for (;;) {
		uint32_t reported = __sync_lock_test_and_set(&counter, 0);
		printf("%d metrics/s\n", reported);
		sleep(1);
		printf("\033[F\033[J");
	}
}

static void *spam_thread(void *_sock)
{
	struct sockaddr_in *si_other = _sock;
	int s, slen = sizeof(*si_other);
	char packet[64];

	if ((s = socket(AF_INET, SOCK_DGRAM, 0))==-1)
		diep("socket");

	for (;;) {
		int len = build_packet(packet);
		if (sendto(s, packet, len, 0, (void *)si_other, slen) < 0)
			printf("C ==> DROPPED\n");

		__sync_add_and_fetch(&counter, 1);
	}

	close(s);
	return NULL;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in si_other;
	pthread_t threads[MAX_THREADS], report;
	int i;

	if (argc != 3) {
		fprintf(stderr, "Usage: 'udp-stress IP PORT'\n");
		exit(-1);
	}

	srand(time(NULL));

	memset(&si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(atoi(argv[2]));
	if (inet_aton(argv[1], &si_other.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	pthread_create(&report, NULL, report_thread, NULL);

	for (i = 0; i < MAX_THREADS; ++i)
		pthread_create(&threads[i], NULL, spam_thread, &si_other);

	for (i =0; i < MAX_THREADS; ++i)
		pthread_join(threads[i], NULL);

	return 0;
}
