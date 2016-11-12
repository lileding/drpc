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
protected:
    inline IOJobBase(char* base, size_t pending) noexcept:
        _base(base), _pending(pending) { }
public:
    inline void reset(char* base, size_t pending) noexcept {
        _base = base;
        _pending = pending;
    }
    IOStatus read(int fd) noexcept;
    IOStatus write(int fd) noexcept;
protected:
    char* _base;
    size_t _pending;
};

template <typename T>
class IOJob : public IOJobBase {
public:
    typedef T element_type;
    typedef T* pointer;
    inline IOJob() noexcept: IOJobBase(reinterpret_cast<char*>(&_p), sizeof(T)) { }
public:
    inline operator T*() noexcept { return &_p; }
    inline T* operator->() noexcept { return &_p; }
    inline static constexpr size_t size() noexcept { return sizeof(T); }
    inline void reset() noexcept {
        _base = reinterpret_cast<char*>(&_p);
        _pending = sizeof(T);
    }
private:
    T _p;
};

template <>
class IOJob<char> : public IOJobBase {
public:
    inline IOJob() noexcept: IOJobBase(nullptr, 0), _buf(nullptr), _sz(0) { }
    ~IOJob() noexcept;
public:
    void reset(size_t sz) noexcept;
    inline operator char*() noexcept { return _buf; }
    inline operator size_t() noexcept { return _sz; }
    inline char* data() noexcept { return _buf; }
    inline size_t size() noexcept { return _sz; }
private:
    char* _buf;
    size_t _sz;
};

} /* namespace serpc */

#endif /* SERPC_SRC_IO_H */

