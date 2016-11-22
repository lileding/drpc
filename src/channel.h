#ifndef SERPC_SRC_CHANNEL_H
#define SERPC_SRC_CHANNEL_H

#include <stdint.h>
#include <queue>
#include "protocol.h"
#include "io.h"

struct kevent;

namespace serpc {

class Queue;

class Channel {
public:
    explicit Channel(int sock) noexcept;
    virtual ~Channel() noexcept;
public:
    inline int ident() noexcept { return _sock; }
    void on_event(Queue* q, struct kevent* ev) noexcept;
    void send(const Message& msg) noexcept;
public:
    virtual bool on_message(const Message& msg) noexcept = 0;
private:
    void on_send() noexcept;
    void on_recv() noexcept;
private:
    int _sock;
    std::queue<Message> _send_queue;
    const Message* _send_buf;
    IOJob _sender;
    bool _send_flag;
    Message _recv_buf;
    IOJob _receiver;
    bool _recv_flag;
};

} /* namespace serpc */

#endif /* SERPC_SRC_CHANNEL_H */

