#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "channel.h"

int drpc_send(int fd, struct iovec* iov) {
    while (iov->iov_len > 0) {
        ssize_t len = write(fd, iov->iov_base, iov->iov_len);
        if (len <= 0) {
            if (errno == EAGAIN) {
                return DRPC_IO_BLOCK;
            } else {
                return DRPC_IO_FAIL;
            }
        } else {
            iov->iov_base += len;
            iov->iov_len -= len;
        }
    }
    return DRPC_IO_COMPLETE;
}

int drpc_recv(int fd, struct iovec* iov) {
    while (iov->iov_len > 0) {
        ssize_t len = read(fd, iov->iov_base, iov->iov_len);
        if (len <= 0) {
            if (errno == EAGAIN) {
                return DRPC_IO_BLOCK;
            } else {
                return DRPC_IO_FAIL;
            }
        } else {
            iov->iov_base += len;
            iov->iov_len -= len;
        }
    }
    return DRPC_IO_COMPLETE;
}

