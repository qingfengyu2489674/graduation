#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "singleCacheHandler.h"
#include "hashTable.h"

ssize_t writeBackCache(int fd, cache* cache) 
{
    printf("Writebake cache is Offset: %zu\n", cache->offset);   
    return pwrite(fd, cache->data, CACHE_SIZE, cache->offset);
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
                size_t writeNumb = writeBackCache(fd ,(cache*)(root->data));
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
                size_t writeNumb = writeBackCache(fd ,(cache*)(root->data));
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


void readWithDevCache(int fd, LRUHash* devHash, cache* cache, void* buf, off_t offsetInCache, size_t count)
{
    size_t cacheSize = CACHE_SIZE;
    
    // 使用 malloc 动态分配缓存缓冲区
    unsigned char* tempBuffer = (unsigned char*)malloc(cacheSize);
    if (tempBuffer == NULL) {
        perror("malloc failed");
        return;
    }

    off_t realOffset = GET_REAL_OFFSET(cache->offset);

    ssize_t ret = pread(fd, tempBuffer, cacheSize, realOffset);
    if (ret < 0) 
    {
        perror("pread from blkcache");
        free(tempBuffer);
        return;
    }

    size_t remainingBytes = count;
    size_t offsetInTempBuffer = offsetInCache % cacheSize;
    if (remainingBytes > cacheSize - offsetInTempBuffer) {
        remainingBytes = cacheSize - offsetInTempBuffer;
    }

    // 从 tempBuffer 复制数据到目标 buf
    memcpy(buf, tempBuffer + offsetInTempBuffer, remainingBytes);

    // 将缓存节点移到哈希表的头部
    moveNodeToHeadByKey(devHash, (long)(cache->offset / CACHE_SIZE));

    // 使用完毕后释放动态分配的缓冲区
    free(tempBuffer);
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

    int ret = lseek(fd, alignedOffset, SEEK_SET);
    if (ret < 0) 
    {
        perror("lseek");
        return;
    } 
    else 
    {
        printf("File offset set to %ld\n", (long)alignedOffset);
    }

    ret = ioctl(fd, IOCTL_READ_BLOCK, buf);
    if (ret < 0) 
    {
        perror("ioctl read second device");
        return;
    }

    checkCacheOverflow(fd);
    createCache(root, devHash, alignedOffset, buf);
}

void writeWithDevCache(int fd, LRUHash* devHash, cache* cache, const void* buf, off_t offsetInCache, size_t count)
{
    size_t cacheSize = CACHE_SIZE;
    
    unsigned char* tempBuffer = (unsigned char*)malloc(cacheSize);
    if (tempBuffer == NULL) 
    {
        perror("malloc failed");
        return;
    }

    off_t realOffset = GET_REAL_OFFSET(cache->offset);

    ssize_t ret = pread(fd, tempBuffer, cacheSize, realOffset);
    if (ret < 0) 
    {
        perror("pread from blkcache");
        free(tempBuffer);
        return;
    }

    size_t remainingBytes = count;
    size_t offsetInTempBuffer = offsetInCache % cacheSize;
    if (remainingBytes > cacheSize - offsetInTempBuffer) 
    {
        remainingBytes = cacheSize - offsetInTempBuffer;
    }

    memcpy(tempBuffer + offsetInTempBuffer, buf, remainingBytes);

    ret = pwrite(fd, tempBuffer, CACHE_SIZE, realOffset);
    if (ret < 0) 
    {
        perror("pwrite from blkcache");
        free(tempBuffer);
        return;
    } 
    
    cache->dirty = 1;
    moveNodeToHeadByKey(devHash, (long)(cache->offset / CACHE_SIZE));

    free(tempBuffer);
}

void writeWithoutDevCache(int fd, const void* buf, off_t offset, size_t count)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* devHash = hashTableFdNode->devHash;

    // -----

    // int writeNumb = pwrite(fd, buf, count, offset);
    // if (writeNumb == -1)
    // {
    //     fprintf(stderr, "Error reading data from file descriptor %d at offset %lld\n", fd, (long long)offset);
    //     return;
    // }

    //TODO:将buf中的内容写到块设备中去

    size_t cacheSize = CACHE_SIZE;
    
    unsigned char* tempBuffer = (unsigned char*)malloc(cacheSize);
    if (tempBuffer == NULL) 
    {
        perror("malloc failed");
        return;
    }

    off_t realOffset = GET_REAL_OFFSET(offset);

    ssize_t ret = pread(fd, tempBuffer, cacheSize, realOffset);
    if (ret < 0) 
    {
        perror("pread from blkcache");
        free(tempBuffer);
        return;
    }

    memcpy(tempBuffer + offset, buf, offset);

    ret = pwrite(fd, tempBuffer, CACHE_SIZE, realOffset);
    if (ret < 0) 
    {
        perror("pwrite from blkcache");
        free(tempBuffer);
        return;
    } 

    off_t alignedOffset = ROUND_DOWN_TO_4096(offset);

    readWithoutDevCache(fd, tempBuffer, alignedOffset);

    free(tempBuffer);
    
}