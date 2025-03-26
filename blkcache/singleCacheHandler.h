#ifndef SINGLE_CACHE_HANDER_H
#define SINGLE_CACHE_HANDER_H


#include "cacheStruct.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>

#define IOCTL_READ_BLOCK  _IOR('b', 2, char*)
#define IOCTL_WRITE_BLOCK _IOW('b', 3, char*)

#define MAX_CACHE_ENTRIES 5

void readWithHostCache(LRUHash* Hash, cache* cache, void* buf, off_t offsetInCache, size_t count);
void readWithoutHostCache(int fd, void* buf, off_t alignedOffset);
void writeHostWithCache(LRUHash* Hash, cache* cache, const void* buf, off_t offsetInCache, size_t count);
void writeHostWithoutCache(int fd, const void* buf, off_t offset, size_t count);

void readDevWithCache(int fd, LRUHash* Hash, cache* cache, void* buf, off_t offsetInCache, size_t count);
void readWithoutDevCache(int fd, void* buf, off_t alignedOffset);
void writeDevWithCache(int fd, LRUHash* Hash, cache* cache, const void* buf, off_t offsetInCache, size_t count);
void writeDevWithoutCache(int fd, const void* buf, off_t offset, size_t count);

void traversalWriteBackCache(AVLTreeNode* root, int fd);
void writeBackAndCleanUpCache(int fd);


#define ROUND_UP_TO_4096(size) (((size) + CACHE_SIZE - 1) / CACHE_SIZE * CACHE_SIZE)
#define ROUND_DOWN_TO_4096(size) ((size) / CACHE_SIZE * CACHE_SIZE)

#endif 