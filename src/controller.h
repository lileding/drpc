#ifndef SERPC_SRC_CONTROLLER_H
#define SERPC_SRC_CONTROLLER_H

#include <mutex>
#include <condition_variable>
#include <serpc.h>

namespace serpc {

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

} /* namespace serpc */

#endif /* SERPC_SRC_CONTROLLER_H */
