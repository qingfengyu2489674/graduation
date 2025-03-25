#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "singleCacheHandler.h"
#include "hashTable.h"

ssize_t writeBackHostCache(int fd, cache* cache) 
{
    printf("Cache writen is Offset: %zu\n", cache->offset);   
    return pwrite(fd, cache->data, CACHE_SIZE, cache->offset);
}

ssize_t writeBackDevCache(int fd, cache* cache) 
{
    printf("DevCache writen is Offset: %zu\n", cache->offset);  
    
}


void traversalWriteBackHostCache(AVLTreeNode* root, int fd)
{
    if (root != NULL) 
    {
        traversalWriteBackHostCache(root->left, fd);

        if(((cache*)(root->data))->dirty)
        {
            //TODO：增加判断逻辑，提供两种写回路径

            if(GET_CACHE_TYPE(((cache*)(root->data))->offset) == CACHE_TYPE_HOST)
            {
                size_t writeNumb = writeBackHostCache(fd ,(cache*)(root->data));
                if (writeNumb == -1)
                {
                    fprintf(stderr, "Write back failed for node with offset %ld\n", (long)((cache*)(root->data))->offset);
                }
                ((cache*)(root->data))->dirty = 0;
            }
        }
        traversalWriteBackHostCache(root->right, fd);
    }
}

void traversalWriteBackDevCache(AVLTreeNode* root, int fd)
{
    if (root != NULL) 
    {
        traversalWriteBackDevCache(root->left, fd);

        if(((cache*)(root->data))->dirty)
        {
            if(GET_CACHE_TYPE(((cache*)(root->data))->offset) == CACHE_TYPE_DEVICE)
            {
                size_t writeNumb = writeBackDevCache(fd ,(cache*)(root->data));
                if (writeNumb == -1)
                {
                    fprintf(stderr, "Write back failed for node with offset %ld\n", (long)((cache*)(root->data))->offset);
                }
                ((cache*)(root->data))->dirty = 0;
            }
        }
        traversalWriteBackDevCache(root->right, fd);
    }
}


void checkCacheOverflow(int fd)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* hostHash = hashTableFdNode->hostHash;

    while(hostHash->size > MAX_CACHE_ENTRIES)
    {
        traversalWriteBackHostCache(*root ,fd);
        deleteTailCache(root, hostHash);
    }

    LRUHash* devHash = hashTableFdNode->devHash;

    while(devHash->size > MAX_CACHE_ENTRIES)
    {
        traversalWriteBackDevCache(*root ,fd);
        deleteTailCache(root, devHash);
    }
}

void readWithHostCache(LRUHash* Hash, cache* cache, void* buf, off_t offsetInCache, size_t count)
{
    memcpy(buf, (cache->data) + offsetInCache, count);
    moveNodeToHeadByKey(Hash, (long)(cache->offset/CACHE_SIZE));
}

void readWithoutHostCache(int fd, void* buf, off_t alignedOffset)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* Hash = hashTableFdNode->hostHash;

    int readNumb = pread(fd, buf, CACHE_SIZE, alignedOffset);

    if (readNumb == -1)
    {
        fprintf(stderr, "Error reading data from file descriptor %d at offset %lld\n", fd, (long long)alignedOffset);
        return;
    }

    checkCacheOverflow(fd);
    createCache(root, Hash, alignedOffset, buf);
}


void writeWithHostCache(LRUHash* Hash, cache* cache, const void* buf, off_t offsetInCache, size_t count)
{
    memcpy((cache->data) + offsetInCache, buf, count);
    cache->dirty = 1;
    moveNodeToHeadByKey(Hash, (long)(cache->offset/CACHE_SIZE));
}

void writeWithoutHostCache(int fd, const void* buf, off_t offset, size_t count)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* Hash = hashTableFdNode->hostHash;

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

    readWithoutHostCache(fd, readBuf, alignedOffset);

    free(readBuf);
}

void writeBackAndCleanUpCache(int fd)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);

    traversalWriteBackHostCache(hashTableFdNode->root ,fd);
    traversalWriteBackDevCache(hashTableFdNode->root ,fd);
    cleanUpCache(hashTableFdNode->root, hashTableFdNode->hostHash, hashTableFdNode->devHash);
}


void readWithDevCache(LRUHash* devHash, cache* cache, void* buf, off_t offsetInCache, size_t count)
{
    // void readWithHostCache(LRUHash* Hash, cache* cache, void* buf, off_t offsetInCache, size_t count)
    // {
    //     memcpy(buf, (cache->data) + offsetInCache, count);
    //     moveNodeToHeadByKey(Hash, (long)(cache->offset/CACHE_SIZE));
    // }

    // TODO:将块设备缓存中的数据读到buf中

    moveNodeToHeadByKey(devHash, (long)(cache->offset/CACHE_SIZE));

}


void readWithoutDevCache(int fd, void* buf, off_t alignedOffset)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* devHash = hashTableFdNode->devHash;

    // int readNumb = pread(fd, buf, CACHE_SIZE, alignedOffset);
    // if (readNumb == -1)
    // {
    //     fprintf(stderr, "Error reading data from file descriptor %d at offset %lld\n", fd, (long long)alignedOffset);
    //     return;
    // }

    //TODO：将数据从块设备中读出

    checkCacheOverflow(fd);
    createCache(root, devHash, alignedOffset, buf);

}

void writeWithDevCache(LRUHash* devHash, cache* cache, const void* buf, off_t offsetInCache, size_t count)
{
    // memcpy((cache->data) + offsetInCache, buf, count);
    // 将数据从buf中写到块设备缓存上

    cache->dirty = 1;
    moveNodeToHeadByKey(devHash, (long)(cache->offset/CACHE_SIZE));
}

void writeWithoutDevCache(int fd, const void* buf, off_t offset, size_t count)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* devHash = hashTableFdNode->devHash;

    // int writeNumb = pwrite(fd, buf, count, offset);
    // if (writeNumb == -1)
    // {
    //     fprintf(stderr, "Error reading data from file descriptor %d at offset %lld\n", fd, (long long)offset);
    //     return;
    // }

    //TODO:将buf中的内容写到块设备中去

    off_t alignedOffset = ROUND_DOWN_TO_4096(offset);

    void* readBuf = (void*)malloc(CACHE_SIZE);
    if (readBuf == NULL) 
    {
        perror("Failed to allocate memory for cache data");
        free(readBuf); 
        return; 
    }

    memcpy(readBuf, buf, CACHE_SIZE);

    readWithoutDevCache(fd, readBuf, alignedOffset);

    free(readBuf);
}