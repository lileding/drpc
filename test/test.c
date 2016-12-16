#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <drpc.h>
#include "../src/protocol.h"

#ifndef ALWAYS_LOOP
#define ALWAYS_LOOP 1
#endif
static int g_run = ALWAYS_LOOP;

static void on_exit(int);

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_exit);
    signal(SIGTERM, on_exit);
    drpc_set_loglevel(DRPC_LOGLEVEL_DEBUG);

    drpc_server_t s = drpc_server_new("localhost", "12321");
    if (!s) {
        return -1;
    }

#if 0
    ::drpc::Client client;
    if (!client) {
        return -1;
    }
    auto chan = client.connect("localhost", "12321");
    drpc_channel_call(chan, "hello", 5, [](const char* response, size_t len) {
        fprintf(stderr, "response %zu\n", len);
        write(STDERR_FILENO, response, len);
        write(STDERR_FILENO, "\n", 1);
    });
#endif

    do {
        sleep(1);
    } while (g_run);
    DRPC_LOG(DEBUG, "test exit");
    drpc_server_drop(s);
    return 0;
}

void on_exit(int sig) {
    g_run = 0;
}

