#include <vector>
#include <utility>
#include <unistd.h>
#include <drpc.h>

int main(int argc, char* argv[]) {
    ::drpc::Server server { "localhost", "12321" };
    if (!server) {
        return -1;
    }

    ::drpc::Client client { "localhost", "12321" };
    if (!client) {
        return -1;
    }

    static const int CALLS = 2;
    std::vector<std::pair<::drpc::Controller, std::string>> calls;
    for (int i = 0; i < CALLS; i++) {
        calls.emplace_back();
        auto& ctx = calls.back();
        std::string req { "hello" };
        req[4] = ('A' + i);
        client.call(&ctx.first, req, &ctx.second);
    }
    for (auto& ctx: calls) {
        ctx.first.wait();
        DRPC_LOG(DEBUG, "got response: \"%s\"", ctx.second.c_str());
    }

    //sleep(1);
    return 0;
}

