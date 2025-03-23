#ifndef SINGLE_CACHE_HANDER_H
#define SINGLE_CACHE_HANDER_H


#include "cacheStruct.h"
#include <fcntl.h>

#define MAX_CACHE_ENTRIES 5

void readWithSingleCache(LRUHash* Hash, cache* cache, void* buf, off_t offsetInCache, size_t count);
void readWithoutSingleCache(int fd, void* buf, off_t alignedOffset);
void writeWithSingleCache(LRUHash* Hash, cache* cache, const void* buf, off_t offsetInCache, size_t count);
void writeWithoutSingleCache(int fd, const void* buf, off_t offset, size_t count);
void inorderTraversalWriteBack(AVLTreeNode* root, int fd);
void writeBackAndCleanUpCache(int fd);


#define ROUND_UP_TO_4096(size) (((size) + CACHE_SIZE - 1) / CACHE_SIZE * CACHE_SIZE)
#define ROUND_DOWN_TO_4096(size) ((size) / CACHE_SIZE * CACHE_SIZE)

#endif 