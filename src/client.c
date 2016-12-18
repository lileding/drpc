#if 0
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <drpc.h>
#include "io.h"
#include "mem.h"
#include "session.h"
#include "channel.h"
#include "client.h"

static void* do_loop(void* arg);

drpc_client_t drpc_client_new() {
    drpc_client_t client = (drpc_client_t)drpc_alloc(sizeof(*client));
    if (!client) {
        return NULL;
    }
    client->quit = drpc_signal_new();
    if (!client->quit) {
        drpc_client_drop(client);
        return NULL;
    }
    if (drpc_event_open(&client->event) != 0) {
        drpc_client_drop(client);
        return NULL;
    }
    if (drpc_event_add(&client->event, drpc_signal_yield(client->quit),
                DRPC_EVENT_READ | DRPC_EVENT_EDGE, NULL) != 0) {
        drpc_client_drop(client);
        return NULL;
    }
    client->thread_num = 1; // FIXME
    client->threads = (pthread_t*)drpc_alloc(sizeof(pthread_t) * client->thread_num);
    if (!client->threads) {
        drpc_client_drop(client);
        return NULL;
    }
    for (size_t i = 0; i < client->thread_num; i++) {
        client->threads[i] = 0;
        int err = pthread_create(client->threads + i, NULL, do_loop, client);
        if (err != 0) {
            DRPC_LOG(ERROR, "pthread_create fail: %s", strerror(err));
            drpc_client_drop(client);
            return NULL;
        }
    }
    STAILQ_INIT(&client->channels);
    client->actives = 0;
    return client;
}

void drpc_client_drop(drpc_client_t client) {
    if (!client) {
        return;
    }
    if (client->quit) {
        drpc_signal_notify(client->quit);
    }
    if (client->threads) {
        for (size_t i = 0; i < client->thread_num; i++) {
            pthread_join(client->threads[i], NULL);
        }
        drpc_free(client->threads);
    }
    drpc_signal_drop(client->quit);
    drpc_event_close(&client->event);
    drpc_free(client);
    DRPC_LOG(DEBUG, "client exit");
}

void* do_loop(void* arg) {
    drpc_client_t client = (drpc_client_t)arg;
    DRPC_LOG(DEBUG, "client works [actives=%zu] [event=%d]",
        client->actives, client->event.kq
    );
    do {
        static const size_t NEVENTS = 1024;
        struct kevent evs[NEVENTS];
        int rv = kevent(client->event.kq, NULL, 0, evs, NEVENTS, NULL);
        if (rv < 0) {
            DRPC_LOG(ERROR, "event fail: %s", strerror(errno));
            break;
        }
        DRPC_LOG(DEBUG, "client got %d event(s)", rv);
        for (int i = 0; i < rv; i++) {
            const struct kevent *ev = &evs[i];
            if (ev->ident == drpc_signal_yield(client->quit)) {
                DRPC_LOG(DEBUG, "client got quit signal");
                break;
            } else {
                drpc_channel_t chan = (drpc_channel_t)ev->udata;
                int64_t actives = drpc_channel_process(chan, ev->filter);
                if (actives == -1) {
                    STAILQ_REMOVE(&client->channels, chan, drpc_channel, entries);
                    drpc_channel_drop(chan);
                    client->actives--;
                }
            }
        }
        DRPC_LOG(DEBUG, "client loop round [actives=%lu]", client->actives);
    } while (client->actives > 0);
    DRPC_LOG(DEBUG, "client loop exit");
    return NULL;
}

drpc_channel_t drpc_client_connect(drpc_client_t client,
        const char* hostname, const char* servname) {
    if (!client) {
        return NULL;
    }
    int fd = drpc_connect(hostname, servname);
    if (fd == -1) {
        return NULL;
    }
    drpc_channel_t chan = drpc_channel_new(fd);
    if (!chan) {
        return NULL;
    }
    chan->client = client;
    if (drpc_event_add(&client->event, chan->endpoint,
                DRPC_EVENT_READ | DRPC_EVENT_WRITE | DRPC_EVENT_EDGE, chan) != 0) {
        drpc_channel_drop(chan);
        return NULL;
    }
    STAILQ_INSERT_TAIL(&client->channels, chan, entries);
    client->actives++;
    return chan;
}
#endif

