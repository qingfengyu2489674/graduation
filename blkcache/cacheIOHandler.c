#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>

#include "singleCacheHandler.h"
#include "hashTable.h"
#include "cacheIOHandler.h"



int openWithCache(const char *pathname, int flags, mode_t mode ,int cacheType)
{
    if (pathname == NULL) 
    {
        fprintf(stderr, "Error: pathname is NULL\n");
        return -1;
    }

    if (table == NULL) 
    {
        table = createHashTableFd();
        if (table == NULL) 
        {
            fprintf(stderr, "Error: Failed to create hash table\n");
            return -1;
        }
    }

    int fd = open(pathname, flags, mode);
    if (fd < 0) 
    {
        perror("Error: Failed to open file");
        return -1;
    }

    LRUHash* Hash = createHash();
    if (Hash == NULL) 
    {
        fprintf(stderr, "Error: Failed to create LRUHash\n");
        close(fd);
        return -1;
    }

    AVLTreeNode* root = NULL;
    createAndInsertFdNode(fd, Hash, root, cacheType);
  
    return fd;
}

int closeWithCache(int fd)
{
    if (fd < 0) 
    {
        fprintf(stderr, "Error: Invalid file descriptor\n");
        return -1;
    }

    HashTableFdNode* hashTableFdNode = findFdNode(fd);
    if (hashTableFdNode == NULL) 
    {
        fprintf(stderr, "Error: Failed to find FD node in hash table\n");
        return -1;
    }

    writeBackAndCleanUpCache(fd);
   
    int deleteResult = deleteFdNode(fd);
    if (deleteResult < 0) {
        fprintf(stderr, "Error: Failed to delete FD node from hash table\n");
        return -1;
    }

    int result = close(fd);
    if (result < 0) {
        perror("Error: Failed to close file");
        return -1;
    }

    if (getFdFromHashTable() < 0) 
    {
        destroyHashTableFd();
        table = NULL;
    }

    return result;
}

ssize_t readWithCache(int fd, void *buf, size_t count, off_t offset)
{

    off_t alignedDownOffset = ROUND_DOWN_TO_4096(offset);
    size_t processedData = 0;

    HashTableFdNode* hashTableFdNode = findFdNode(fd);
   
    for(int i = 0; processedData < count ;i++)
    {
        size_t steppedAlignedOffset = alignedDownOffset + i * CACHE_SIZE;
        size_t DataToProcess = ((steppedAlignedOffset + CACHE_SIZE) > (offset + count))? (count - processedData): MIN((steppedAlignedOffset + CACHE_SIZE - offset), CACHE_SIZE);
    
        cache* cache = findCache(hashTableFdNode->root, steppedAlignedOffset);
        size_t offsetInCache = (offset > steppedAlignedOffset) ? (offset - steppedAlignedOffset) : 0;

        if(hashTableFdNode->cacheType == CACHE_TYPE_HOST)
        {
            if(cache != NULL)
            {
                readWithHostCache(hashTableFdNode->Hash, cache, buf + processedData, offsetInCache, DataToProcess);
            }
            else
            {
                void* bufOut = (void*)malloc(CACHE_SIZE);
                readWithoutHostCache(fd, bufOut, steppedAlignedOffset);
                memcpy(buf + processedData, bufOut + offsetInCache, DataToProcess);
                free(bufOut);
            }
        }
        
        else
        {
            if(cache != NULL)
            {
                readDevWithCache(fd, hashTableFdNode->Hash, cache, buf + processedData, offsetInCache, DataToProcess);
            }
            else
            {
                void* bufOut = (void*)malloc(CACHE_SIZE);
                readWithoutDevCache(fd, bufOut, steppedAlignedOffset);
                memcpy(buf + processedData, bufOut + offsetInCache, DataToProcess);
                free(bufOut);
            }
        }
        processedData = processedData + DataToProcess;

    }
    return processedData;

}

ssize_t writeWithCache(int fd, const void *buf, size_t count, off_t offset)
{
    HashTableFdNode* hashTableFdNode = findFdNode(fd);

    off_t alignedDownOffset = ROUND_DOWN_TO_4096(offset);
    size_t processedData = 0;
    
    for(int i = 0; processedData < count ;i++)
    {

        size_t steppedAlignedOffset = alignedDownOffset + i * CACHE_SIZE;
        size_t DataToProcess = ((steppedAlignedOffset + CACHE_SIZE) > (offset + count))? (count - processedData): MIN((steppedAlignedOffset + CACHE_SIZE - offset), CACHE_SIZE);
        size_t offsetInCache = (offset > steppedAlignedOffset) ? (offset - steppedAlignedOffset) : 0;
        size_t offsetOutCache = (offset >= steppedAlignedOffset) ? offset : steppedAlignedOffset;
        
        cache* cache = findCache(hashTableFdNode->root, steppedAlignedOffset);

        if(hashTableFdNode->cacheType == CACHE_TYPE_HOST)
        {
            if(cache != NULL)
            {
                writeHostWithCache(hashTableFdNode->Hash, cache, buf + processedData, offsetInCache, DataToProcess);
            }
            else
            {
                writeHostWithoutCache(fd, buf + processedData, offsetOutCache, DataToProcess);
            }
        }
        else
        {
            if(cache != NULL)
            {
                writeDevWithCache(fd, hashTableFdNode->Hash, cache, buf + processedData, offsetInCache, DataToProcess);
            }
            else
            {
                writeDevWithoutCache(fd, buf + processedData, offsetOutCache, DataToProcess);
            }
        }
        processedData = processedData + DataToProcess;

    }
    return processedData;

}


