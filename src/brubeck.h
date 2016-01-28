#ifndef __BRUBECK__H_
#define __BRUBECK__H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define _GNU_SOURCE
#include <math.h>
#undef _GNU_SOURCE

#define BRUBECK_STATS 1
#define MAX_ADDR 256

typedef double value_t;
typedef uint64_t hash_t;

struct brubeck_server;
struct brubeck_metric;

#include "jansson.h"
#include "log.h"
#include "utils.h"
#include "slab.h"
#include "histogram.h"
#include "metric.h"
#include "sampler.h"
#include "backend.h"
#include "ht.h"
#include "server.h"

#endif
