#ifndef DRPC_SRC_CONTROLLER_H
#define DRPC_SRC_CONTROLLER_H

#include <mutex>
#include <condition_variable>
#include <drpc.h>

namespace drpc {

class ControllerImpl {
public:
    ControllerImpl() noexcept: _buf(nullptr) { }
    ~ControllerImpl() noexcept { unlock(); }
public:
    void wait() noexcept;
    void lock(std::string* buf) noexcept;
    void unlock() noexcept;
    std::string* buffer() noexcept { return _buf; };
private:
    std::string* _buf;
    std::mutex _mtx;
    std::condition_variable _cv;
};

} /* namespace drpc */

#endif /* DRPC_SRC_CONTROLLER_H */
