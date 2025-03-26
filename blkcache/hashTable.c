#include <stdlib.h>
#include <stdio.h>

#include "cacheStruct.h"
#include "hashTable.h"

HashTableFd* table = NULL;

HashTableFd* createHashTableFd(void) 
{
    HashTableFd* table = (HashTableFd*)malloc(sizeof(HashTableFd));
    table->size = HASH_FD_SIZE;
    table->buckets = (HashTableFdNode**)calloc(HASH_FD_SIZE, sizeof(HashTableFdNode*));
    pthread_mutex_init(&(table->lock), NULL);

    return table;
}


HashTableFdNode* createHashTableFdNode(int fd, LRUHash* Hash, AVLTreeNode* root, int cacheType) 
{
    HashTableFdNode* node = (HashTableFdNode*)malloc(sizeof(HashTableFdNode));
    node->fd = fd;
    node->Hash = Hash;
    node->cacheType = cacheType;
    node->next = NULL;
    return node;
}

void createAndInsertFdNode(int fd, LRUHash* hash, AVLTreeNode* root, int cacheType) 
{
    int index = HASH_FD(fd, table->size);
    HashTableFdNode* newNode = createHashTableFdNode(fd, hash, root, cacheType);

    pthread_mutex_lock(&(table->lock)); 

    newNode->next = table->buckets[index];
    table->buckets[index] = newNode;

    pthread_mutex_unlock(&(table->lock)); 
}


HashTableFdNode* findFdNode(int fd) 
{
    int index = HASH_FD(fd, table->size);
    HashTableFdNode* currentNode = table->buckets[index];

    while (currentNode != NULL) {
        if (currentNode->fd == fd) 
        {
            return currentNode;
        }
        currentNode = currentNode->next;
    }
    return NULL;
}

int getFdFromHashTable(void) 
{
    for (int i = 0; i < table->size; i++) 
    {
        HashTableFdNode* currentNode = table->buckets[i];

        if (currentNode != NULL) 
        {
            return currentNode->fd;
        }
    }
    return -1;
}

int deleteFdNode(int fd) 
{
    int index = HASH_FD(fd, table->size);

    pthread_mutex_lock(&(table->lock));

    HashTableFdNode* currentNode = table->buckets[index];
    HashTableFdNode* previousNode = NULL;

    while (currentNode != NULL) 
    {
        if (currentNode->fd == fd) 
        {
            if (previousNode == NULL) 
            {
                table->buckets[index] = currentNode->next;
            } else 
            {
                previousNode->next = currentNode->next;
            }
            free(currentNode);

            pthread_mutex_unlock(&(table->lock));

            return 1;
        }
        previousNode = currentNode;
        currentNode = currentNode->next;
    }

    pthread_mutex_unlock(&(table->lock));

    return 0;
}


void clearHashTable(void) 
{
    int fd;

    while ((fd = getFdFromHashTable()) >= 0) 
    {
        deleteFdNode(fd);
    }
}

void destroyHashTableFd(void) 
{
    clearHashTable();

    free(table->buckets);
    pthread_mutex_destroy(&(table->lock));
    free(table);
}


void printHashTable(void) 
{
    printf("HashTable contents:\n");
    for (int i = 0; i < table->size; i++) 
    {
        HashTableFdNode* currentNode = table->buckets[i];
        if (currentNode != NULL) 
        {
            printf("Bucket %d: ", i);
            while (currentNode != NULL) 
            {
                printf("%d ", currentNode->fd);
                currentNode = currentNode->next;
            }
            printf("\n");
        }
    }
}

