#ifndef SERPC_SRC_SIGNAL_H
#define SERPC_SRC_SIGNAL_H

namespace serpc {

class Signal {
public:
    Signal() noexcept;
    ~Signal() noexcept;
    inline operator bool() noexcept { return _fildes[0] != -1; }
public:
    inline int sender() noexcept { return _fildes[1]; }
    inline int receiver() noexcept { return _fildes[0]; }
    void notify() noexcept;
private:
    int _fildes[2];
};

} /* namespace serpc */

#endif /* SERPC_SRC_SIGNAL_H */

