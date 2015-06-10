#ifndef __GH_LOG_H__
#define __GH_LOG_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

const char *gh_log_instance(void);
void gh_log_set_instance(const char *instance);
void gh_log_open(const char *path);
void gh_log_reopen(void);
void gh_log_die(void) __attribute__ ((noreturn));
void gh_log_write(const char *message, ...) 
	__attribute__((format (printf, 1, 2)));

#define log_splunk(M, ...) gh_log_write("instance=%s " M "\n", gh_log_instance(), ##__VA_ARGS__);

#define log_splunk_errno(M, ...) log_splunk( \
	M " errno=%d msg=\"%s\"", ##__VA_ARGS__, errno, strerror(errno));

#ifdef NDEBUG
#define debug(M, ...)
#else
#define debug(M, ...) gh_log_write("[DBG]: " M "\n", ##__VA_ARGS__)
#endif

#define die(M, ...) do {\
	fprintf(stderr, "[FATAL]: " M "\n", ##__VA_ARGS__); \
	gh_log_die(); \
} while(0)

#endif
