#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>

#include "brubeck.h"

#ifdef __gnu_linux__
#define LARGE_SOCK_SIZE 33554431
#else
#define LARGE_SOCK_SIZE 4096
#endif

char *find_substr(const char *s, const char *find, size_t slen)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == '\0' || slen-- < 1)
					return NULL;
			} while (sc != c);

			if (len > slen)
				return NULL;
				
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return (char *)s;
}

void sock_setnonblock(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	flags |= O_NONBLOCK;

	if (fcntl(fd, F_SETFL, flags) < 0)
		die("Failed to set O_NONBLOCK");
}

void sock_setreuse(int fd, int reuse)
{
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
		die("Failed to set SO_REUSEADDR");
}

void sock_enlarge_in(int fd)
{
	int bs = LARGE_SOCK_SIZE;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bs, sizeof(bs)) == -1)
		die("Failed to set SO_RCVBUF");
}

void sock_enlarge_out(int fd)
{
	int bs = LARGE_SOCK_SIZE;

	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bs, sizeof(bs)) == -1)
		die("Failed to set SO_SNDBUF");
}

void sock_setreuse_port(int fd, int reuse)
{
#ifdef SO_REUSEPORT
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) == -1)
		die("failed to set SO_REUSEPORT");
#endif
}

void url_to_inaddr2(struct sockaddr_in *addr, const char *url, int port)
{
	memset(addr, 0x0, sizeof(struct sockaddr_in));

	if (url) {
		struct addrinfo hints;
		struct addrinfo *result, *rp;

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_family = AF_INET;

		if (getaddrinfo(url, NULL, &hints, &result) != 0)
			die("failed to resolve address '%s'", url);

		/* Look for the first IPv4 address we can find */
		for (rp = result; rp; rp = rp->ai_next) {
			if (result->ai_family == AF_INET &&
				result->ai_addrlen == sizeof(struct sockaddr_in))
				break;
		}

		if (!rp)
			die("address format not supported");

		memcpy(addr, rp->ai_addr, rp->ai_addrlen);
		addr->sin_port = htons(port);

		freeaddrinfo(result);
	} else {
		addr->sin_family = AF_INET;
		addr->sin_port = htons(port);
		addr->sin_addr.s_addr = htonl(INADDR_ANY);
	}
}

#define FLOAT_PRECISION 4

int brubeck_itoa(char *ptr, uint64_t number)
{
	char *origin = ptr;
	int size;

	do {
		*ptr++ = '0' + (number % 10);
		number /= 10;
	} while (number);

	size = ptr - origin;
	ptr--;

	while (origin < ptr) {
		char t = *ptr;
		*ptr-- = *origin;
		*origin++ = t;
	}

	return size;
}

int brubeck_ftoa(char *outbuf, float f)
{
	uint64_t mantissa, int_part, frac_part;
	int safe_shift;
	uint64_t safe_mask;
	short exp2;
	char *p;

	union {
		int L;
		float F;
	} x;

	x.F = f;
	p = outbuf;

	exp2 = (unsigned char)(x.L >> 23) - 127;
	mantissa = (x.L & 0xFFFFFF) | 0x800000;
	frac_part = 0;
	int_part = 0;

	if (x.L < 0) {
		*p++ = '-';
	}

	if (exp2 < -36) {
		*p++ = '0';
		goto END;
	}

	safe_shift = -(exp2 + 1);
	safe_mask = 0xFFFFFFFFFFFFFFFFULL >>(64 - 24 - safe_shift);

	if (exp2 >= 64) {
		int_part = ULONG_MAX;
	} else if (exp2 >= 23) {
		int_part = mantissa << (exp2 - 23);
	} else if (exp2 >= 0) {
		int_part = mantissa >> (23 - exp2);
		frac_part = (mantissa) & safe_mask;
	} else /* if (exp2 < 0) */ {
		frac_part = (mantissa & 0xFFFFFF);
	}

	if (int_part == 0) {
		*p++ = '0';
	} else {
		p += brubeck_itoa(p, int_part);
	}
 
	if (frac_part != 0) {
		int m;

		*p++ = '.';

		for (m = 0; m < FLOAT_PRECISION; m++) {
			frac_part = (frac_part << 3) + (frac_part << 1); 
			*p++ = (frac_part >> (24 + safe_shift)) + '0';
			frac_part &= safe_mask;
		}

		for (; p[-1] == '0'; --p) {}

		if (p[-1] == '.') {
			--p;
		}
	}

END:
	*p = 0;
	return p - outbuf;
}
