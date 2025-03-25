#include <stdio.h>
#include <stdlib.h>

#include "lru.h"


LRUHash* createHash() 
{
    LRUHash* Hash = (LRUHash*)malloc(sizeof(LRUHash));
    Hash->size = 0;
    Hash->lruHead = Hash->lruTail = NULL;

    for (int i = 0; i < CAPACITY; i++) 
    {
        Hash->hashTable[i] = NULL;
    }
    return Hash;
}

lruNode* createLRUNode(long key, LRUHash* Hash) 
{
   
    lruNode* newNode = (lruNode*)malloc(sizeof(lruNode));
    newNode->key = key;
    newNode->lruPre = newNode->lruNext = NULL;

    hashNode* newHashNode = (hashNode*)malloc(sizeof(hashNode));
    newHashNode->curNode = newNode;
    newHashNode->hashNext = NULL;

    int hashIndex = HASH_FUNCTION(key);

    if (Hash->hashTable[hashIndex] == NULL) 
    {
        Hash->hashTable[hashIndex] = newHashNode;
    } 
    
    else 
    {
        hashNode* current = Hash->hashTable[hashIndex];
        while (current->hashNext != NULL) 
        {
            current = current->hashNext;
        }
        current->hashNext = newHashNode;
    }

    return newNode;
}

void addToHead(LRUHash* Hash, lruNode* node) 
{
    if (Hash->lruHead == NULL) 
    {
        Hash->lruHead = Hash->lruTail = node;
    } 
    else 
    {
        node->lruNext = Hash->lruHead;
        Hash->lruHead->lruPre = node;
        Hash->lruHead = node;
    }
    Hash->size++;
}

void createAndAddLRUNode(long key, LRUHash* Hash) 
{

    lruNode* newNode = createLRUNode(key, Hash);
    addToHead(Hash, newNode);
    return;
}


void moveToHead(LRUHash* Hash, lruNode* node) 
{
    if (Hash->lruHead == node) 
    {
        return;
    }

    if (Hash->lruTail == node) 
    {
        Hash->lruTail = node->lruPre;
    }

    if (node->lruNext) 
    {
        node->lruNext->lruPre = node->lruPre;
    }
    if (node->lruPre) 
    {
        node->lruPre->lruNext = node->lruNext;
    }

    node->lruNext = Hash->lruHead;
    node->lruPre = NULL;

    if (Hash->lruHead) 
    {
        Hash->lruHead->lruPre = node;
    }
    Hash->lruHead = node;

    if (Hash->lruTail == NULL) {
        Hash->lruTail = node;
    }
}

lruNode* searchLRUNodeByKey(LRUHash* Hash, long key) 
{
    int hashIndex = HASH_FUNCTION(key);

    hashNode* current = Hash->hashTable[hashIndex];
    while (current != NULL) {
        if (current->curNode->key == key) {
            return current->curNode; 
        }
        current = current->hashNext;
    }

    return NULL; 
}

void deleteLRUNode(LRUHash* Hash, lruNode* node) 
{
    if (node == NULL || Hash == NULL) {
        return;
    }

    if (Hash->lruHead == node) {
        Hash->lruHead = node->lruNext;
    }

    if (Hash->lruTail == node) {
        Hash->lruTail = node->lruPre;
    }

    if (node->lruPre != NULL) {
        node->lruPre->lruNext = node->lruNext;
    }

    if (node->lruNext != NULL) {
        node->lruNext->lruPre = node->lruPre;
    }

    Hash->size--;

    printf("Hash->size = %d\n", Hash->size);

    node->lruPre = node->lruNext = NULL;

    return;
}

void deleteLRUNodeByKey(LRUHash* Hash, long key) 
{
    if (Hash == NULL) 
    {
        fprintf(stderr, "Error: Hash table is NULL\n");
        return;
    }

    lruNode* nodeToDelete = searchLRUNodeByKey(Hash, key);
    if (nodeToDelete == NULL) 
    {
        fprintf(stderr, "Error: Node with key %ld not found\n", key);
        return; 
    }

    int hashIndex = HASH_FUNCTION(key);

    hashNode* current = Hash->hashTable[hashIndex];
    hashNode* prev = NULL;
    while (current != NULL && current->curNode != nodeToDelete) 
    {
        prev = current;
        current = current->hashNext;
    }

    if (current == NULL) 
    {
        fprintf(stderr, "Error: Node not found in hash chain\n");
        return;
    }

    if (prev == NULL) 
    {
        Hash->hashTable[hashIndex] = current->hashNext;
    } 
    else 
    {
        prev->hashNext = current->hashNext;
    }

    free(current);
    current = NULL;

    deleteLRUNode(Hash, nodeToDelete);
    free(nodeToDelete);
    nodeToDelete = NULL;

}

long deleteLRUTail(LRUHash* Hash) 
{
    if (Hash == NULL) {
        fprintf(stderr, "Error: Hash table is NULL\n");
        return -1;
    }

    long tailKey = GET_LRU_TAIL_KEY(Hash);
    if (tailKey == -1) {
        fprintf(stderr, "Error: LRU tail is NULL\n");
        return -1;
    }

    deleteLRUNodeByKey(Hash, tailKey);

    return tailKey; 
}

void moveNodeToHeadByKey(LRUHash* Hash, long key) 
{
    if (Hash == NULL) {
        fprintf(stderr, "Error: Hash table is NULL\n");
        return;
    }

    lruNode* node = searchLRUNodeByKey(Hash, key);
    if (node == NULL) {
        fprintf(stderr, "Error: Node with key %ld not found\n", key);
        return;
    }

    moveToHead(Hash, node);
}

void freeHash(LRUHash* Hash)
{
    while (Hash->lruTail != NULL) 
    {
        long temp_key = deleteLRUTail(Hash);
    }
    free(Hash);
    Hash = NULL;
}

void printLRUList(LRUHash* hash) 
{
    if (hash == NULL) return;
    lruNode* current = hash->lruHead;
   
    printf("LRU 链表大小 %d\n",hash->size);
    printf("LRU 链表内容：\n");

    while (current != NULL) 
    {
        printf("%ld ", current->key);
        current = current->lruNext;
    }
    printf("\n");
}


/*
int main() {
    // 创建哈希表
    LRUHash* cache = createHash();
    if (cache == NULL) {
        fprintf(stderr, "Error: Failed to create hash table\n");
        return 1;
    }

    // 测试 createAndAddNode 函数
    printf("测试 createAndAddNode 函数:\n");
    createAndAddNode(10, cache);
    createAndAddNode(20, cache);
    createAndAddNode(30, cache);
    createAndAddNode(40, cache);
    printf("插入 10, 20, 30, 40 后的 LRU 链表: ");
    lruNode* current = cache->lruHead;
    while (current != NULL) {
        printf("%ld ", current->key);
        current = current->lruNext;
    }
    printf("\n");

    // 测试 moveNodeToHeadByKey 函数
    printf("测试 moveNodeToHeadByKey 函数:\n");
    moveNodeToHeadByKey(cache, 20);
    printf("移动 20 到头部后的 LRU 链表: ");
    current = cache->lruHead;
    while (current != NULL) {
        printf("%ld ", current->key);
        current = current->lruNext;
    }
    printf("\n");

    // 测试 deleteNodeByKey 函数
    printf("测试 deleteNodeByKey 函数:\n");
    deleteNodeByKey(cache, 30);
    printf("删除 30 后的 LRU 链表: ");
    current = cache->lruHead;
    while (current != NULL) {
        printf("%ld ", current->key);
        current = current->lruNext;
    }
    printf("\n");

    // 测试 deleteLRUTail 函数
    printf("测试 deleteLRUTail 函数:\n");
    long tailKey = deleteLRUTail(cache);
    printf("删除尾部节点后的 LRU 链表: ");
    current = cache->lruHead;
    while (current != NULL) {
        printf("%ld ", current->key);
        current = current->lruNext;
    }
    printf("\n尾部被删除的节点的 key: %ld\n", tailKey);

    // 清理内存
    while (cache->lruTail != NULL) 
    {
        long temp_key = deleteLRUTail(cache);
    }
    free(cache);

    return 0;
}
*/