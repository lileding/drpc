#ifndef DRPC_SRC_IO_H
#define DRPC_SRC_IO_H

#include <sys/uio.h>

#define DRPC_IO_COMPLETE  0
#define DRPC_IO_BLOCK     1
#define DRPC_IO_FAIL      2

int drpc_write(int fd, struct iovec* iov);
int drpc_read(int fd, struct iovec* iov);

int drpc_listen(const char* hostname, const char* servname, int backlog);
int drpc_connect(const char* hostname, const char* servname);

int drpc_set_nonblock(int fd);

#endif /* DRPC_SRC_IO_H */

