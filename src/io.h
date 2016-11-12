#ifndef SERPC_SRC_IO_H
#define SERPC_SRC_IO_H

#include <stddef.h>

namespace serpc {

enum IOStatus {
    DONE = 0,
    ERROR = 1,
    CONTINUE = 2,
};

class IOJobBase {
public:
    inline IOJobBase(char* base, size_t pending) noexcept:
        _base(base), _pending(pending) { }
public:
    inline void reset(char* base, size_t pending) noexcept {
        _base = base;
        _pending = pending;
    }
    IOStatus read(int fd) noexcept;
    IOStatus write(int fd) noexcept;
private:
    char* _base;
    size_t _pending;
};

template <size_t N>
class IOJob : public IOJobBase {
public:
    inline IOJob(): IOJobBase(_buf, N) { }
    inline explicit IOJob(size_t pending): IOJobBase(_buf, pending) { }
public:
    inline operator char*() noexcept { return _buf; }
    inline char* data() noexcept { return _buf; }
    inline static constexpr size_t size() noexcept { return N; }
private:
    char _buf[N];
};

template <>
class IOJob<0> : public IOJobBase {
public:
    inline IOJob(const char* buf, size_t pending) noexcept:
        IOJobBase(const_cast<char*>(buf), pending) { }
};

} /* namespace serpc */

#endif /* SERPC_SRC_IO_H */

