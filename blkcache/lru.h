#ifndef LRU_H
#define LRU_H


#define CAPACITY 4  
#define HASH_FUNCTION(key) ((key) % CAPACITY)
#define GET_LRU_TAIL_KEY(hash) ((hash)->lruTail ? (hash)->lruTail->key : -1)


typedef struct lruNode
{
    long key;
    struct lruNode* lruPre;
    struct lruNode* lruNext;
} lruNode;

typedef struct hashNode
{
    lruNode *curNode;
    struct hashNode *hashNext;  
} hashNode;

typedef struct 
{
    int size;
    lruNode *lruHead;
    lruNode *lruTail;
    hashNode *hashTable[CAPACITY];
} LRUHash;


LRUHash *createHash();
void createAndAddLRUNode(long key, LRUHash *Hash);
void moveNodeToHeadByKey(LRUHash *Hash, long key);
void deleteLRUNodeByKey(LRUHash *Hash, long key);
long deleteLRUTail(LRUHash *Hash);
void freeHash(LRUHash *Hash);
void printLRUList(LRUHash *hash);

#endif