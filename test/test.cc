#include <signal.h>
#include <vector>
#include <utility>
#include <unistd.h>
#include <drpc.h>

static bool g_run = true;

static void on_exit(int) noexcept;

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, on_exit);
    signal(SIGTERM, on_exit);

    drpc_set_loglevel(DRPC_LOGLEVEL_DEBUG);
    ::drpc::Server server { "localhost", "12321" };
    if (!server) {
        return -1;
    }

    while (g_run) {
        sleep(1);
    }
    DRPC_LOG(DEBUG, "test exit");
    return 0;
}

void on_exit(int) noexcept {
    g_run = false;
}

