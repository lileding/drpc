#ifndef DRPC_SRC_SIGNAL_H
#define DRPC_SRC_SIGNAL_H

namespace drpc {

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

} /* namespace drpc */

#endif /* DRPC_SRC_SIGNAL_H */

