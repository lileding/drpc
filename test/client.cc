#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <vector>
#include <thread>
#include <atomic>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <drpc.h>
extern "C" {
#include "../src/io.h"
#include "../src/protocol.h"
}

static void do_send(int sock, std::atomic<uint64_t>& refcnt) noexcept;
static void do_recv(int sock, std::atomic<uint64_t>& refcnt, std::atomic<uint64_t>& counter) noexcept;

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);
    drpc_set_loglevel(DRPC_LOGLEVEL_DEBUG);
    std::vector<std::thread> thrs;
    static const int NTHREADS = 1;
    std::atomic<uint64_t> refcnt { 0 };
    std::atomic<uint64_t> counter { 0 };
    for (int i = 0; i < NTHREADS; i++) {
        int sock = drpc_connect("127.0.0.1", "12321");
        if (sock == -1) {
            continue;
        }
        fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) & ~O_NONBLOCK);
        refcnt++;
        thrs.emplace_back(do_send, sock, std::ref(refcnt));
        refcnt++;
        thrs.emplace_back(do_recv, sock, std::ref(refcnt), std::ref(counter));
    }
    while (refcnt) {
        sleep(1);
        auto qps = counter.exchange(0);
        DRPC_LOG(DEBUG, "%lu qps", qps);
    }
    for (auto& t: thrs) {
        t.join();
    }
    return 0;
}

void do_send(int sock, std::atomic<uint64_t>& refcnt) noexcept {
    while (1) {
        drpc_request_header h = {
            0, 0, 0, 0, 0xfacebabe, 5,
        };
        static const char* body = "hello";
        ssize_t rv = write(sock, &h, sizeof(h));
        if (rv != sizeof(h)) {
            DRPC_LOG(DEBUG, "write header fail [ret=%ld]: %s", rv, strerror(errno));
            break;
        }
        rv = write(sock, body, h.payload);
        if (rv != h.payload) {
            DRPC_LOG(DEBUG, "write body fail [ret=%ld]: %s", rv, strerror(errno));
            break;
        }
    }
    shutdown(sock, SHUT_WR);
    refcnt--;
}

void do_recv(int sock, std::atomic<uint64_t>& refcnt, std::atomic<uint64_t>& counter) noexcept {
    char buf[5] = {'\0'};
    while (1) {
        drpc_response_header h;
        ssize_t rv = read(sock, &h, sizeof(h));
        if (rv != sizeof(h)) {
            DRPC_LOG(DEBUG, "read header fail [ret=%ld]: %s", rv, strerror(errno));
            break;
        }
        rv = read(sock, buf, h.payload);
        if (rv != h.payload) {
            DRPC_LOG(DEBUG, "read body fail [ret=%ld]: %s", rv, strerror(errno));
            break;
        }
        counter++;
    }
    shutdown(sock, SHUT_RD);
    refcnt--;
}
