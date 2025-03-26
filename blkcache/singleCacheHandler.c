#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "singleCacheHandler.h"
#include "hashTable.h"

ssize_t writeBackCache(int fd, cache* cache) 
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    if(hashTableFdNode->cacheType == CACHE_TYPE_HOST)
    {
        printf("Writebake host cache is Offset: %zu\n", cache->offset);   
        return pwrite(fd, cache->data, CACHE_SIZE, cache->offset);
    }
    else
    {
        printf("Writebake dev cache is Offset: %zu\n", cache->offset); 

        unsigned char* tempBuffer = (unsigned char*)malloc(CACHE_SIZE);
        if (tempBuffer == NULL) 
        {
            perror("malloc failed");
            return;
        }
    
        int readNumb = pread(fd, tempBuffer, CACHE_SIZE, cache->offset);

        if (readNumb == -1)
        {
            fprintf(stderr, "Error reading data from file descriptor %d at offset %lld\n", fd, (long long)cache->offset);
            return;
        }

        int ret = lseek(fd, cache->offset, SEEK_SET);
        if (ret < 0) 
        {
            perror("lseek");
            return;
        } 
    
        ret = ioctl(fd, IOCTL_READ_BLOCK, tempBuffer);
        if (ret < 0) 
        {
            perror("ioctl read second device");
            return;
        }
    }

}

void traversalWriteBackCache(AVLTreeNode* root, int fd)
{
    if (root != NULL) 
    {
        traversalWriteBackCache(root->left, fd);
        if(((cache*)(root->data))->dirty)
        {
            size_t writeNumb = writeBackCache(fd ,(cache*)(root->data));
            if (writeNumb == -1)
            {
                fprintf(stderr, "Write back failed for node with offset %ld\n", (long)((cache*)(root->data))->offset);
            }
            ((cache*)(root->data))->dirty = 0;
        }
        traversalWriteBackCache(root->right, fd);
    }
}


void checkCacheOverflow(int fd)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* hostHash = hashTableFdNode->Hash;

    while(hostHash->size > MAX_CACHE_ENTRIES)
    {
        traversalWriteBackCache(*root ,fd);
        deleteTailCache(root, hostHash);
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
    LRUHash* Hash = hashTableFdNode->Hash;

    int readNumb = pread(fd, buf, CACHE_SIZE, alignedOffset);

    if (readNumb == -1)
    {
        fprintf(stderr, "Error reading data from file descriptor %d at offset %lld\n", fd, (long long)alignedOffset);
        return;
    }

    checkCacheOverflow(fd);
    createCache(root, Hash, alignedOffset, buf);
}


void writeHostWithCache(LRUHash* Hash, cache* cache, const void* buf, off_t offsetInCache, size_t count)
{
    memcpy((cache->data) + offsetInCache, buf, count);
    cache->dirty = 1;
    moveNodeToHeadByKey(Hash, (long)(cache->offset/CACHE_SIZE));
}

void writeHostWithoutCache(int fd, const void* buf, off_t offset, size_t count)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* Hash = hashTableFdNode->Hash;

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

    traversalWriteBackCache(hashTableFdNode->root ,fd);
    cleanUpCache(hashTableFdNode->root, hashTableFdNode->Hash, hashTableFdNode->Hash);
}


void readDevWithCache(int fd, LRUHash* devHash, cache* cache, void* buf, off_t offsetInCache, size_t count)
{
    size_t cacheSize = CACHE_SIZE;
    
    // 使用 malloc 动态分配缓存缓冲区
    unsigned char* tempBuffer = (unsigned char*)malloc(cacheSize);
    if (tempBuffer == NULL) {
        perror("malloc failed");
        return;
    }

    ssize_t ret = pread(fd, tempBuffer, cacheSize, cache->offset);
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

    memcpy(buf, tempBuffer + offsetInTempBuffer, remainingBytes);

    moveNodeToHeadByKey(devHash, (long)(cache->offset / CACHE_SIZE));

    free(tempBuffer);
}


void readWithoutDevCache(int fd, void* buf, off_t alignedOffset)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* devHash = hashTableFdNode->Hash;

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

void writeDevWithCache(int fd, LRUHash* devHash, cache* cache, const void* buf, off_t offsetInCache, size_t count)
{
    size_t cacheSize = CACHE_SIZE;
    
    unsigned char* tempBuffer = (unsigned char*)malloc(cacheSize);
    if (tempBuffer == NULL) 
    {
        perror("malloc failed");
        return;
    }

    ssize_t ret = pread(fd, tempBuffer, cacheSize, cache->offset);
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

    ret = pwrite(fd, tempBuffer, CACHE_SIZE, cache->offset);
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

void writeDevWithoutCache(int fd, const void* buf, off_t offset, size_t count)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    AVLTreeNode** root = &(hashTableFdNode->root);
    LRUHash* devHash = hashTableFdNode->Hash;


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

    off_t alignedOffset = ROUND_DOWN_TO_4096(offset);

    int ret = lseek(fd, alignedOffset, SEEK_SET);
    if (ret < 0) 
    {
        perror("lseek");
        return;
    } 

    ret = ioctl(fd, IOCTL_READ_BLOCK, tempBuffer);
    if (ret < 0) 
    {
        perror("ioctl read second device");
        return;
    }

    memcpy(tempBuffer + offset, buf, count);

    // ret = pwrite(fd, tempBuffer, CACHE_SIZE, alignedOffset);
    // if (ret < 0) 
    // {
    //     perror("pwrite from blkcache");
    //     free(tempBuffer);
    //     return;
    // } 

    checkCacheOverflow(fd);
    createCache(root, devHash, alignedOffset, tempBuffer);

    free(tempBuffer);
    
}