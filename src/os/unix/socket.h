#ifndef NGNIX_OS_UNIX_SOCKET_H
#define NGNIX_OS_UNIX_SOCKET_H

#include <stdint.h>

int ng_socket_listen(uint16_t port);
int ng_socket_nonblocking(int fd);

#endif
