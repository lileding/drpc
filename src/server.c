#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "io.h"
#include "mem.h"
#include "server.h"
#include "channel.h"

static void do_event(void* arg);
static void on_accept(drpc_server_t server);
static void on_quit(drpc_server_t server);

drpc_server_t drpc_server_new(const char* hostname, const char* servname) {
    DRPC_ENSURE_OR(hostname && servname, NULL, "invalid argument");
    drpc_server_t server = (drpc_server_t)drpc_alloc(sizeof(*server));
    if (!server) {
        return NULL;
    }
    server->execute = (drpc_task_func)do_event;
    TAILQ_INIT(&server->channels);
    server->actives = 0;
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
    if (drpc_thrpool_open(&server->pool, 4) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    if (drpc_event_open(&server->event) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    if (drpc_event_add(&server->event, drpc_signal_yield(server->quit),
                DRPC_EVENT_READ | DRPC_EVENT_EDGE, NULL) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    server->actives++;
    if (drpc_event_add(&server->event, server->endpoint,
                DRPC_EVENT_READ, NULL) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    server->actives++;
    drpc_thrpool_apply(&server->pool, (drpc_task_t)server);
    return server;
}

void drpc_server_drop(drpc_server_t server) {
    if (!server) {
        return;
    }
    if (server->quit) {
        drpc_signal_notify(server->quit);
    }
    drpc_thrpool_close(&server->pool);
    drpc_thrpool_join(&server->pool);
    drpc_signal_drop(server->quit);
    drpc_event_close(&server->event);
    if (server->endpoint != -1) {
        close(server->endpoint);
    }
    drpc_free(server);
    DRPC_LOG(DEBUG, "server exit");
}

void do_event(void* arg) {
    drpc_server_t server = (drpc_server_t)arg;
    DRPC_LOG(DEBUG, "server wait [endpoint=%d] [event=%d] [actives=%zu]",
        server->endpoint, server->event.kq, server->actives
    );
    static const size_t NEVENTS = 1024;
    struct kevent evs[NEVENTS];
    int rv = kevent(server->event.kq, NULL, 0, evs, NEVENTS, NULL);
    if (rv < 0) {
        DRPC_LOG(ERROR, "event fail: %s", strerror(errno));
        drpc_thrpool_close(&server->pool);
    }
    for (int i = 0; i < rv; i++) {
        const struct kevent *ev = &evs[i];
        if (ev->ident == drpc_signal_yield(server->quit)) {
            on_quit(server);
        } else if (ev->ident == server->endpoint) {
            on_accept(server);
        } else {
            drpc_channel_t chan = (drpc_channel_t)ev->udata;
            if (drpc_channel_process(chan, ev->filter) != 0) {
                DRPC_LOG(DEBUG, "channel fail [endpoint=%d]", chan->endpoint);
                TAILQ_REMOVE(&server->channels, chan, entries);
                server->actives--;
                drpc_channel_drop(chan);
            }
#if 0
            drpc_channel_t chan = (drpc_channel_t)ev->udata;
            if (ev->filter & EVFILT_READ) {
                drpc_channel_read(chan);
            }
            if (ev->filter & EVFILT_WRITE) {
                drpc_channel_write(chan);
            }
#endif
        }
    }
    if (server->actives > 0) {
        drpc_thrpool_apply(&server->pool, (drpc_task_t)server);
    }
}

void on_accept(drpc_server_t server) {
    int sock = -1;
    while (1) {
        sock = accept(server->endpoint, NULL, NULL);
        if (sock == -1) {
            if (errno == EAGAIN) {
                return;
            }
            break;
        }
        drpc_channel_t chan = drpc_channel_new(sock);
        if (!chan) {
            close(sock);
            continue;
        }
        chan->pool = &server->pool;
        if (drpc_event_add(&server->event, sock,
                    DRPC_EVENT_READ | DRPC_EVENT_WRITE | DRPC_EVENT_EDGE,
                    chan) != 0) {
            drpc_channel_drop(chan);
            continue;
        }
        TAILQ_INSERT_TAIL(&server->channels, chan, entries);
        server->actives++;
        DRPC_LOG(DEBUG, "server accept sock=%d", sock);
#if 0
        drpc_channel_t chan = drpc_channel_new(sock);
        if (!chan) {
            close(sock);
            continue;
        }
        if (drpc_event_add(&server->event, sock,
                    DRPC_EVENT_READ | DRPC_EVENT_WRITE | DRPC_EVENT_EDGE,
                    chan) != 0) {
            drpc_channel_drop(chan);
            continue;
        }
        STAILQ_INSERT_HEAD(&server->channels, chan, entries);
        server->actives++;
        DRPC_LOG(DEBUG, "server accept sock=%d", sock);
#endif
    }
    DRPC_LOG(ERROR, "accept fail: %s", strerror(errno));
    close(server->endpoint);
    server->endpoint = -1;
}

void on_quit(drpc_server_t server) {
    drpc_signal_drop(server->quit);
    server->quit = NULL;
    server->actives--;
    close(server->endpoint);
    server->endpoint = -1;
    server->actives--;
    drpc_thrpool_close(&server->pool);
}

