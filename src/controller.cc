#include <drpc.h>
#include "controller.h"

namespace drpc {

Controller::Controller() noexcept {
    _impl = new ControllerImpl();
}

Controller::~Controller() noexcept {
    delete _impl;
}

void Controller::wait() noexcept {
    _impl->wait();
}

void ControllerImpl::wait() noexcept {
    std::unique_lock<decltype(_mtx)> lk { _mtx };
    _cv.wait(lk);
}

void ControllerImpl::lock(std::string* buf) noexcept {
    std::unique_lock<decltype(_mtx)> lk { _mtx };
    _buf = buf;
}

void ControllerImpl::unlock() noexcept {
    _cv.notify_one();
}

} /* namespace drpc */

