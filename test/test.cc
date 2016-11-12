#include <stdlib.h>
#include <future>
#include <serpc.h>

int main(int argc, char* argv[]) {
    ::serpc::Server server { "localhost", "12321" };
    if (!server) {
        return EXIT_FAILURE;
    }

    std::async([]() {
        ::serpc::Client client { "localhost", "12321" };
        SERPC_ENSURE(static_cast<bool>(client), "create client fail");
        client.invoke([](int code) {});
        client.invoke([](int code) {});
        client.invoke([](int code) {});
        client.invoke([](int code) {});
    }).wait();

    return 0;
}

