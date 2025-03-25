#ifndef SINGLE_CACHE_HANDER_H
#define SINGLE_CACHE_HANDER_H


#include "cacheStruct.h"
#include <fcntl.h>

#define CACHE_TYPE_HOST 0
#define CACHE_TYPE_DEVICE 1

#define CACHE_TYPE_MASK ((off_t)1 << (sizeof(off_t)*8 - 1))
#define GET_CACHE_TYPE(off) (((off) & CACHE_TYPE_MASK) ? 1 : 0)
#define GET_REAL_OFFSET(off) ((off) & ~CACHE_TYPE_MASK)
#define SET_CACHE_TYPE(off, type) \
    ((off) = ((off) & ~CACHE_TYPE_MASK) | ((type) ? CACHE_TYPE_MASK : 0))

#define MAX_CACHE_ENTRIES 5

void readWithHostCache(LRUHash* Hash, cache* cache, void* buf, off_t offsetInCache, size_t count);
void readWithoutHostCache(int fd, void* buf, off_t alignedOffset);
void writeWithHostCache(LRUHash* Hash, cache* cache, const void* buf, off_t offsetInCache, size_t count);
void writeWithoutHostCache(int fd, const void* buf, off_t offset, size_t count);
void readWithDevCache(LRUHash* Hash, cache* cache, void* buf, off_t offsetInCache, size_t count);
void readWithoutDevCache(int fd, void* buf, off_t alignedOffset);
void writeWithDevCache(LRUHash* Hash, cache* cache, const void* buf, off_t offsetInCache, size_t count);
void writeWithoutDevCache(int fd, const void* buf, off_t offset, size_t count);

void traversalWriteBackHostCache(AVLTreeNode* root, int fd);
void writeBackAndCleanUpCache(int fd);


#define ROUND_UP_TO_4096(size) (((size) + CACHE_SIZE - 1) / CACHE_SIZE * CACHE_SIZE)
#define ROUND_DOWN_TO_4096(size) ((size) / CACHE_SIZE * CACHE_SIZE)

#endif 