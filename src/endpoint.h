#ifndef DRPC_SRC_ENDPOINT_H
#define DRPC_SRC_ENDPOINT_H

int drpc_listen(const char* hostname, const char* servname, int backlog);

int drpc_connect(const char* hostname, const char* servname);

int drpc_set_nonblock(int fd);

#endif /* DRPC_SRC_ENDPOINT_H */

