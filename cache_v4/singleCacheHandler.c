#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "singleCacheHandler.h"
#include "hashTable.h"

ssize_t writeToCache(int fd, cache* cache) 
{
    printf("Cache writen is Offset: %zu\n", cache->offset);   
    return pwrite(fd, cache->data, CACHE_SIZE, cache->offset);
}


void inorderTraversalWriteBack(AVLTreeNode* root, int fd)
 {
    if (root != NULL) 
    {
        inorderTraversalWriteBack(root->left, fd);

        if(((cache*)(root->data))->dirty)
        {
            size_t writeNumb = writeToCache(fd ,(cache*)(root->data));
            if (writeNumb == -1)
            {
                fprintf(stderr, "Write back failed for node with offset %ld\n", (long)((cache*)(root->data))->offset);
            }
            ((cache*)(root->data))->dirty = 0;
        }

        inorderTraversalWriteBack(root->right, fd);
    }
}

void checkCacheOverflow(int fd)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* Hash = hashTableFdNode->hash;

    while(Hash->size > MAX_CACHE_ENTRIES)
    {
        inorderTraversalWriteBack(*root ,fd);
        deleteTailCache(root, Hash);
    }
}

void readWithSingleCache(LRUHash* Hash, cache* cache, void* buf, off_t offsetInCache, size_t count)
{

    memcpy(buf, (cache->data) + offsetInCache, count);
    moveNodeToHeadByKey(Hash, (long)(cache->offset/CACHE_SIZE));
}

void readWithoutSingleCache(int fd, void* buf, off_t alignedOffset)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* Hash = hashTableFdNode->hash;

    int readNumb = pread(fd, buf, CACHE_SIZE, alignedOffset);

    if (readNumb == -1)
    {
        fprintf(stderr, "Error reading data from file descriptor %d at offset %lld\n", fd, (long long)alignedOffset);
        return;
    }

    checkCacheOverflow(fd);
    createCache(root, Hash, alignedOffset, buf);
}


void writeWithSingleCache(LRUHash* Hash, cache* cache, const void* buf, off_t offsetInCache, size_t count)
{
    memcpy((cache->data) + offsetInCache, buf, count);
    cache->dirty = 1;
    moveNodeToHeadByKey(Hash, (long)(cache->offset/CACHE_SIZE));
}

void writeWithoutSingleCache(int fd, const void* buf, off_t offset, size_t count)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* Hash = hashTableFdNode->hash;

    int writeNumb = pwrite(fd, buf, count, offset);

    if (writeNumb == -1)
    {
        fprintf(stderr, "Error reading data from file descriptor %d at offset %lld\n", fd, (long long)offset);
        return;
    }

    off_t alignedOffset = ROUND_DOWN_TO_4096(offset);

    void* readBuf = (void*)malloc(CACHE_SIZE);
    if (readBuf == NULL) 
    {
        perror("Failed to allocate memory for cache data");
        free(readBuf); 
        return; 
    }

    memcpy(readBuf, buf, CACHE_SIZE);

    readWithoutSingleCache(fd, readBuf, alignedOffset);

    free(readBuf);
}

void writeBackAndCleanUpCache(int fd)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);

    inorderTraversalWriteBack(hashTableFdNode->root ,fd);
    cleanUpCache(hashTableFdNode->root, hashTableFdNode->hash);
}

