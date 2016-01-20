GIT_SHA = $(shell git rev-parse --short HEAD)
TARGET = brubeck
LIBS = -lm -pthread -lrt -lcrypto -ljansson
CC = gcc
CXX = g++
CFLAGS = -g -Wall -O3 -Wno-strict-aliasing -Isrc -Ivendor/ck/include -DNDEBUG=1 -DGIT_SHA=\"$(GIT_SHA)\"

.PHONY: default all clean

default: $(TARGET)
all: default

SOURCES = \
	src/backend.c \
	src/backends/carbon.c \
	src/bloom.c \
	src/city.c \
	src/histogram.c \
	src/ht.c \
	src/http.c \
	src/internal_sampler.c \
	src/log.c \
	src/metric.c \
	src/sampler.c \
	src/samplers/statsd-secure.c \
	src/samplers/statsd.c \
	src/server.c \
	src/setproctitle.c \
	src/slab.c \
	src/utils.c

ifndef BRUBECK_NO_HTTP
	LIBS += -lmicrohttpd
	CFLAGS += -DBRUBECK_HAVE_MICROHTTPD
endif

CFLAGS += -DBRUBECK_METRICS_FLOW

OBJECTS = $(patsubst %.c, %.o, $(SOURCES))
HEADERS = $(wildcard src/*.h) $(wildcard src/libcuckoo/*.h)

TEST_SRC = $(wildcard tests/*.c)
TEST_OBJ = $(patsubst %.c, %.o, $(TEST_SRC))

%.o: %.c $(HEADERS) vendor/ck/src/libck.a
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS) brubeck.o
	$(CC) -flto brubeck.o $(OBJECTS) $(LIBS) vendor/ck/src/libck.a -o $@.new
	mv $@.new $@

$(TARGET)_test: $(OBJECTS) $(TEST_OBJ)
	$(CC) $(OBJECTS) $(TEST_OBJ) $(LIBS) vendor/ck/src/libck.a -o $@

test: $(TARGET)_test
	./$(TARGET)_test

vendor/ck/Makefile:
	cd vendor/ck && ./configure

vendor/ck/src/libck.a: vendor/ck/Makefile
	$(MAKE) -C vendor/ck

clean:
	-rm -f $(OBJECTS) brubeck.o
	-rm -f $(TEST_OBJ)
	-rm -f $(TARGET) $(TARGET)_test
