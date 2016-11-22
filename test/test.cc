#include <vector>
#include <utility>
#include <unistd.h>
#include <serpc.h>

int main(int argc, char* argv[]) {
    ::serpc::Server server { "localhost", "12321" };
    if (!server) {
        return -1;
    }

    ::serpc::Client client { "localhost", "12321" };
    if (!client) {
        return -1;
    }

    std::vector<std::pair<::serpc::Controller, std::string>> calls;
    for (char x = 'A'; x < 'D'; x++) {
        calls.emplace_back();
        auto& ctx = calls.back();
        std::string req { "hello" };
        req[4] = x;
        client.call(&ctx.first, req, &ctx.second);
    }
    for (auto& ctx: calls) {
        ctx.first.wait();
        SERPC_LOG(DEBUG, "got response: \"%s\"", ctx.second.c_str());
    }

    //sleep(1);
    return 0;
}

