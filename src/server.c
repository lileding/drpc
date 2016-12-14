#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "mem.h"
#include "server.h"
#include "endpoint.h"
#include "channel.h"

static void* do_loop(void* arg);
static int on_accept(drpc_server_t server);

drpc_server_t drpc_server_new(const char* hostname, const char* servname) {
    DRPC_ENSURE_OR(hostname && servname, NULL, "invalid argument");
    drpc_server_t server = (drpc_server_t)drpc_alloc(sizeof(*server));
    if (!server) {
        return NULL;
    }
    server->endpoint = drpc_listen(hostname, servname, 1024);
    if (server->endpoint == -1) {
        drpc_server_drop(server);
        return NULL;
    }
    server->quit = drpc_signal_new();
    if (!server->quit) {
        drpc_server_drop(server);
        return NULL;
    }
    if (drpc_queue_open(&server->queue) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    if (drpc_queue_add(&server->queue, drpc_signal_yield(server->quit),
                DRPC_FILT_READ | DRPC_EVENT_EDGE, NULL) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    server->thread = 0;
    if (pthread_create(&server->thread, NULL, do_loop, server) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    if (drpc_queue_add(&server->queue, server->endpoint,
                DRPC_FILT_READ, NULL) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    STAILQ_INIT(&server->channels);
    server->actives = 0;
    return server;
}

void drpc_server_drop(drpc_server_t server) {
    if (!server) {
        return;
    }
    if (server->quit) {
        drpc_signal_notify(server->quit);
    }
    if (server->thread != 0) {
        pthread_join(server->thread, NULL);
    }
    drpc_signal_drop(server->quit);
    drpc_queue_close(&server->queue);
    if (server->endpoint != -1) {
        close(server->endpoint);
    }
    drpc_free(server);
    DRPC_LOG(DEBUG, "server exit");
}

void* do_loop(void* arg) {
    drpc_server_t server = (drpc_server_t)arg;
    DRPC_LOG(DEBUG, "server works [endpoint=%d] [queue=%d]", server->endpoint, server->queue.kq);
    while (server->endpoint != -1 || server->actives > 0) {
        static const size_t NEVENTS = 1024;
        struct kevent evs[NEVENTS];
        int rv = kevent(server->queue.kq, NULL, 0, evs, NEVENTS, NULL);
        if (rv < 0) {
            DRPC_LOG(ERROR, "queue fail: %s", strerror(errno));
            break;
        }
        for (int i = 0; i < rv; i++) {
            const struct kevent *ev = &evs[i];
            if (ev->ident == drpc_signal_yield(server->quit)) {
                DRPC_LOG(DEBUG, "server got quit signal [endpoint=%d] [actives=%lu]", server->endpoint, server->actives);
                close(server->endpoint);
                server->endpoint = -1;
                drpc_channel_t chan = NULL;
                STAILQ_FOREACH(chan, &server->channels, entries) {
                    drpc_channel_shutdown_read(chan);
                }
            } else if (ev->ident == server->endpoint) {
                if (on_accept(server) == -1) {
                    close(server->endpoint);
                    server->endpoint = -1;
                }
            } else {
                drpc_channel_t chan = (drpc_channel_t)ev->udata;
                int64_t actives = drpc_channel_process(chan, ev->filter);
                if (actives == -1) {
                    STAILQ_REMOVE(&server->channels, chan, drpc_channel, entries);
                    drpc_channel_drop(chan);
                    server->actives--;
                } else if (actives == 0 && server->endpoint == -1) {
                    DRPC_LOG(DEBUG, "channel exit [endpoint=%d]", chan->endpoint);
                    //STAILQ_REMOVE(&server->channels, chan, drpc_channel, entries);
                    drpc_channel_drop(chan);
                    server->actives--;
                }
            }
        }
        DRPC_LOG(DEBUG, "server loop round [endpoint=%d] [actives=%lu]", server->endpoint, server->actives);
    }
    DRPC_LOG(DEBUG, "server loop exit");
    return NULL;
}

int on_accept(drpc_server_t server) {
    if (server->endpoint == -1) {
        return 0;
    }
    int sock = -1;
    while (1) {
        sock = accept(server->endpoint, NULL, NULL);
        if (sock == -1) {
            if (errno == EAGAIN) {
                return 0;
            }
            DRPC_LOG(ERROR, "accept fail: %s", strerror(errno));
            return -1;
        }
        drpc_channel_t chan = drpc_channel_new(sock);
        if (!chan) {
            close(sock);
            continue;
        }
        if (drpc_queue_add(&server->queue, sock,
                    DRPC_FILT_READ | DRPC_FILT_WRITE | DRPC_EVENT_EDGE,
                    chan) != 0) {
            drpc_channel_drop(chan);
            continue;
        }
        STAILQ_INSERT_HEAD(&server->channels, chan, entries);
        server->actives++;
        DRPC_LOG(DEBUG, "server accept sock=%d", sock);
    }
    return -1;
}

