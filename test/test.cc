#include <signal.h>
#include <vector>
#include <utility>
#include <unistd.h>
#include <drpc.h>

using ::drpc::Server;
using ::drpc::Client;
using ::drpc::Controller;
using ::drpc::String;

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    Server server { "localhost", "12321" };
    if (!server) {
        return -1;
    }

#if 0
    Client client { "localhost", "12321" };
    if (!client) {
        return -1;
    }
    Client client { "localhost", "12321" };
    if (!client) {
        return -1;
    }

    static const int CALLS = 2;
    std::vector<std::pair<Controller, String>> calls;
    for (int i = 0; i < CALLS; i++) {
        calls.emplace_back();
        auto& ctx = calls.back();
        String req { "hello" };
        req[4] = ('A' + i);
        client.call(&ctx.first, req, &ctx.second);
    }
    for (auto& ctx: calls) {
        auto scope = ctx.first.wait();
        DRPC_LOG(DEBUG, "got response: \"%s\"", ctx.second.c_str());
    }
#endif

    //sleep(1);
    return 0;
}

