#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <drpc.h>

static volatile int g_run = 1;

static void on_exit(int);
static void work(drpc_round_t round, void* arg,
        const char* request, uint32_t reqlen,
        char** response, uint32_t* resplen);

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    struct sigaction sa;
    sa.sa_handler = on_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER | SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    //drpc_set_loglevel(DRPC_LOGLEVEL_DEBUG);
    drpc_set_loglevel(DRPC_LOGLEVEL_NOTICE);

    drpc_server_t s = drpc_server_new("localhost", "12321");
    if (!s) {
        return -1;
    }
    drpc_server_register(s, work, NULL);
    while (g_run) {
        sleep(1);
    }
    drpc_server_join(s);
    return 0;
}

void on_exit(int sig) {
    g_run = 0;
}

void work(drpc_round_t round, void* arg,
        const char* request, uint32_t reqlen,
        char** response, uint32_t* resplen) {
#if 1
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

