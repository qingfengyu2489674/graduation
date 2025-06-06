#ifndef CACHE_STRUCT_H
#define CACHE_STRUCT_H

#include <stdbool.h>

#include "AVLTree.h"
#include "lru.h"


typedef struct cache
{
    off_t offset;
    void* data;
    bool dirty;
}cache;

#define CACHE_SIZE 512

void createCache(AVLTreeNode **root, LRUHash *Hash, off_t offset, void *data);
cache* findCache(AVLTreeNode *root, off_t offset);
void deleteTailCache(AVLTreeNode **root, LRUHash *Hash);
void cleanUpCache(AVLTreeNode *root, LRUHash *host_hash, LRUHash *devHash);

void printAVLTreeTraversalResults(AVLTreeNode *root, LRUHash *Hash);

#endif 