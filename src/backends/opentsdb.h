#ifndef __BRUBECK_OPENTSDB_H__
#define __BRUBECK_OPENTSDB_H__

struct brubeck_opentsdb {
        struct brubeck_backend backend;

        int out_sock;
        struct sockaddr_in out_sockaddr;

        const char* tags;
        size_t sent;
};

struct brubeck_backend *brubeck_opentsdb_new(
        struct brubeck_server *server, json_t *setings, int shard_n);

#endif
