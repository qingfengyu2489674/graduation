#ifndef CACHE_IO_HANDLER_H
#define CACHE_IO_HANDLER_H

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))


int openWithCache(const char *pathname, int flags, mode_t mode);
int closeWithCache(int fd);
ssize_t readWithCache(int fd, void *buf, size_t count, off_t offset);
ssize_t writeWithCache(int fd, const void *buf, size_t count, off_t offset);

#endif