#include <stddef.h>
#include <string.h>
#include "brubeck.h"

static inline int is_connected(struct brubeck_opentsdb *self)
{
        return (self->out_sock >= 0);
}

static int opentsdb_connect(void *backend)
{
        struct brubeck_opentsdb *self = (struct brubeck_opentsdb *)backend;

        if (is_connected(self))
                return 0;

        self->out_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

        if (self->out_sock >= 0) {
                int rc = connect(self->out_sock,
                                 (struct sockaddr *)&self->out_sockaddr,
                                 sizeof(self->out_sockaddr));

                if (rc == 0) {
                        log_splunk("backend=opentsdb event=connected");
                        sock_enlarge_out(self->out_sock);
                        return 0;
                }

                close(self->out_sock);
                self->out_sock = -1;
        }

        log_splunk_errno("backend=opentsdb event=failed_to_connect");
        return -1;
}

static void opentsdb_disconnect(struct brubeck_opentsdb *self)
{
        log_splunk_errno("backend=opentsdb event=disconnected");

        close(self->out_sock);
        self->out_sock = -1;
}

static void opentsdb_write(
        const char *key,
        value_t value,
        void *backend)
{
        struct brubeck_opentsdb *opentsdb = (struct brubeck_opentsdb *)backend;
        char buffer[1024];
        char *ptr = buffer;
        size_t key_len = strlen(key);
        ssize_t wr;

        if (!is_connected(opentsdb))
                return;

        strcpy(ptr, "put");
        ptr += strlen("put");
        *ptr++ = ' ';
        
        memcpy(ptr, key, key_len);
        ptr += key_len;
        *ptr++ = ' ';

        ptr += brubeck_itoa(ptr, opentsdb->backend.tick_time);
        *ptr++ = ' ';

        ptr += brubeck_ftoa(ptr, value);
        *ptr++ = ' ';

        strcpy(ptr, opentsdb->tags);
        ptr += strlen(opentsdb->tags);
        *ptr++ = '\n';

        wr = write_in_full(opentsdb->out_sock, buffer, ptr - buffer);
        if (wr < 0) {
                opentsdb_disconnect(opentsdb);
                return;
        }

        opentsdb->sent += wr;
}

struct brubeck_backend *
brubeck_opentsdb_new(struct brubeck_server *server, json_t *settings, int shard_n)
{
        struct brubeck_opentsdb *opentsdb = xcalloc(1, sizeof(struct brubeck_opentsdb));
        char *address;
        int port, frequency = 0;

        json_unpack_or_die(settings,
                "{s:s, s:i, s:i, s:s}",
                "address", &address,
                "port", &port,
                "frequency", &frequency,
                "tags", &(opentsdb->tags));

        opentsdb->backend.type = BRUBECK_BACKEND_OPENTSDB;
        opentsdb->backend.shard_n = shard_n;
        opentsdb->backend.connect = &opentsdb_connect;

        opentsdb->backend.sample = &opentsdb_write;
        opentsdb->backend.flush = NULL;

        opentsdb->backend.sample_freq = frequency;
        opentsdb->backend.server = server;
        opentsdb->out_sock = -1;
        url_to_inaddr2(&opentsdb->out_sockaddr, address, port);

        brubeck_backend_run_threaded((struct brubeck_backend *)opentsdb);
        log_splunk("backend=opentsdb event=started");

        return (struct brubeck_backend *)opentsdb;
}
