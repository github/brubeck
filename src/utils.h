#ifndef __BRUBECK_H__
#define __BRUBECK_H__

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)
#define ct_assert(e) ((void)sizeof(char[1 - 2*!(e)]))

void url_to_inaddr2(struct sockaddr_in *addr, const char *url, int port);

void sock_setnonblock(int fd);
void sock_setreuse(int fd, int reuse);
void sock_setreuse_port(int fd, int reuse);
void sock_enlarge_out(int fd);
void sock_enlarge_in(int fd);

char *find_substr(const char *s, const char *find, size_t slen);

int brubeck_itoa(char *ptr, uint64_t number);
int brubeck_ftoa(char *outbuf, float f);

static inline int starts_with(const char *str, const char *prefix)
{
	for (; ; str++, prefix++)
		if (!*prefix)
			return 1;
		else if (*str != *prefix)
			return 0;
}

static inline void *xmalloc(size_t size)
{
	void *ptr = malloc(size);

	if (unlikely(ptr == NULL))
		die("oom");

	return ptr;
}

static inline void *xcalloc(size_t n, size_t size)
{
	void *ptr = calloc(n, size);

	if (unlikely(ptr == NULL))
		die("oom");

	return ptr;
}

static inline void *xrealloc(void *ptr, size_t size)
{
	void *new_ptr = realloc(ptr, size);

	if (unlikely(new_ptr == NULL))
		die("oom");

	return new_ptr;
}

#define brubeck_atomic_inc(P) __sync_add_and_fetch((P), 1)
#define brubeck_atomic_dec(P) __sync_add_and_fetch((P), -1)
#define brubeck_atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define brubeck_atomic_swap(P, V) __sync_lock_test_and_set((P), (V))
#define brubeck_atomic_fetch(P) __sync_add_and_fetch((P), 0)

/* Compile read-write barrier */
#define brubeck_barrier() __sync_synchronize()

void initproctitle (int argc, char **argv);
int getproctitle(char **procbuffer);
void setproctitle (const char *prog, const char *txt);

static inline ssize_t xwrite(int fd, const void *buf, size_t len)
{
	ssize_t nr;
	while (1) {
		nr = write(fd, buf, len);
		if ((nr < 0) && (errno == EAGAIN || errno == EINTR))
			continue;
		return nr;
	}
}

static inline ssize_t write_in_full(int fd, const void *buf, size_t count)
{
	const char *p = buf;
	ssize_t total = 0;

	while (count > 0) {
		ssize_t written = xwrite(fd, p, count);
		if (written < 0)
			return -1;
		if (!written) {
			errno = ENOSPC;
			return -1;
		}
		count -= written;
		p += written;
		total += written;
	}

	return total;
}

#define json_unpack_or_die(json, fmt, ...) { \
	json_error_t _error_j; \
	if (json_unpack_ex(json, &_error_j, 0, fmt, __VA_ARGS__) < 0) \
		die("config error: %s", _error_j.text); }

extern uint32_t CityHash32(const char *s, size_t len);

#endif
