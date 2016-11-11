#ifndef SERPC_H
#define SERPC_H

#include <stdio.h>

#define SERPC_LOG(level, fmt, ...) \
    fprintf(stderr, "SERPC " #level " " fmt "\n", ##__VA_ARGS__)

namespace serpc {

class Engine {
public:
    Engine() noexcept;
    ~Engine() noexcept;
public:
    void run() noexcept;
};

} /* namespace serpc */

#endif /* SERPC_H */

