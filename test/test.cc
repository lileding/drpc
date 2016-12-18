#include <stdio.h>
#include <future>
#include <signal.h>
#include <unistd.h>
#include <drpc.h>

static int g_run = 1;

static void on_exit(int);
static void work(drpc_round_t round, void* arg,
        const char* request, uint32_t reqlen,
        char** response, uint32_t* resplen);

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_exit);
    signal(SIGTERM, on_exit);
    //drpc_set_loglevel(DRPC_LOGLEVEL_DEBUG);
    drpc_set_loglevel(DRPC_LOGLEVEL_NOTICE);

    ::drpc::Server s { "localhost", "12321" };
    if (!s) {
        return -1;
    }

    //atomic_int_least64_t counter = 0;
    drpc_server_register(s, work, nullptr);//&counter);

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
    while (g_run) {
        sleep(1);
    }
    return 0;
}

void on_exit(int sig) {
    g_run = 0;
}

void work(drpc_round_t round, void* arg,
        const char* request, uint32_t reqlen,
        char** response, uint32_t* resplen) {
#if 0
    *resplen = reqlen;
    *response = (char*)malloc(reqlen);
    strncpy(*response, request, reqlen);
    drpc_round_complete(round, DRPC_STATUS_OK);
#else
    std::async([round, arg, request, reqlen, response, resplen]() {
        struct timespec ts = { 0, 1000000 };
        nanosleep(&ts, nullptr);
        *resplen = reqlen;
        *response = (char*)malloc(reqlen);
        strncpy(*response, request, reqlen);
        drpc_round_complete(round, DRPC_STATUS_OK);
    });
#endif
}

