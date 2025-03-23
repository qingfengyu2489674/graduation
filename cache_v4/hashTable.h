#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <pthread.h>

#include "AVLTree.h"
#include "lru.h"

#define HASH_FD(fd, size) ((fd) % (size))
#define HASH_FD_SIZE 5

typedef struct HashTableFdNode 
{
    int fd;
    LRUHash* hash;
    AVLTreeNode* root;         
    struct HashTableFdNode* next;
} HashTableFdNode;

typedef struct HashTableFd 
{
    int size;
    HashTableFdNode** buckets;
    pthread_mutex_t lock;
} HashTableFd;


HashTableFd* createHashTableFd(void);
void createAndInsertFdNode(int fd, LRUHash* hash, AVLTreeNode* root);
HashTableFdNode* findFdNode(int fd);
int deleteFdNode(int fd);
int getFdFromHashTable(void);
void destroyHashTableFd(void);
void printHashTable(void);

extern HashTableFd* table;

#endif 