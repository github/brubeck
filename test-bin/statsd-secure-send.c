#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#define DEBUG
#define HMAC_KEY "750c783e6ab0b503eaa86e310a5db738"
#define SHA_SIZE 32

static void diep(char *s)
{
	perror(s);
	exit(1);
}

static int build_packet(char *buffer, const char *metric)
{
	HMAC_CTX ctx;
	struct timespec now;
	uint64_t timestamp;
	uint32_t nonce;
	unsigned int hmac_len;
	int metric_len = strlen(metric);

	RAND_pseudo_bytes((void *)&nonce, sizeof(nonce));
	clock_gettime(CLOCK_REALTIME, &now);
	timestamp = (uint64_t)now.tv_sec - 2; /* fake delay */

	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, HMAC_KEY, strlen(HMAC_KEY), EVP_sha256(), NULL);

	HMAC_Update(&ctx, (void *)&timestamp, sizeof(timestamp));
	HMAC_Update(&ctx, (void *)&nonce, sizeof(nonce));
	HMAC_Update(&ctx, (void *)metric, metric_len);

	HMAC_Final(&ctx, buffer, &hmac_len);

	HMAC_CTX_cleanup(&ctx);

	memcpy(buffer + SHA_SIZE, &timestamp, sizeof(timestamp));
	memcpy(buffer + SHA_SIZE + 8, &nonce, sizeof(nonce));
	memcpy(buffer + SHA_SIZE + 12, metric, metric_len);

#ifdef DEBUG
	{
		int i;

		fprintf(stderr, "HMAC: ");
		for (i = 0; i < SHA_SIZE; ++i)
			fprintf(stderr, "%02x", (unsigned char)buffer[i]);

		fprintf(stderr, "\nTIMESTAMP: %llu\nNONCE: %08x\nMETRIC: %s\n",
			(long long unsigned int)timestamp, nonce, metric);
	}
#endif

	return SHA_SIZE + 12 + metric_len;
}

int main(int argc, char *argv[])
{
	struct sockaddr_in si_other;
	int s, len;
	char buffer[1024];

	if (argc != 4) {
		fprintf(stderr, "Usage: 'udp-stress IP PORT METRIC'\n");
		exit(-1);
	}

	srand(time(NULL));

	memset(&si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(atoi(argv[2]));
	if (inet_aton(argv[1], &si_other.sin_addr) == 0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		diep("socket");

	len = build_packet(buffer, argv[3]);
	sendto(s, buffer, len, 0, (void *)&si_other, sizeof(si_other));

	close(s);

	return 0;
}
