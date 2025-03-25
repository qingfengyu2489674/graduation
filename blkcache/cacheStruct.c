#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cacheStruct.h"

void cleanUpAVLTreeData(AVLTreeNode** root) 
{
    if (root == NULL || *root == NULL) 
    {
        return;
    }

    cleanUpAVLTreeData(&((*root)->left));

    cleanUpAVLTreeData(&((*root)->right));

    if ((*root) != NULL && (*root)->data != NULL) 
    {
        cache* cacheData = (cache*)(*root)->data;

        if (cacheData != NULL && cacheData->data != NULL) 
        {
            free(cacheData->data);
            cacheData->data = NULL;
        }
    }
}

void cleanUpCache(AVLTreeNode* root, LRUHash* hostHash, LRUHash* devHash) 
{
    
    if (hostHash != NULL) 
    {
        freeHash(hostHash);
        hostHash = NULL;
    }

    if (devHash != NULL) 
    {
        freeHash(devHash);
        devHash = NULL;
    }

    if(root != NULL)
    {
        cleanUpAVLTreeData(&root); 
        cleanUpAVLTree(&root); 
    }

}


void createCache(AVLTreeNode** root, LRUHash* Hash, off_t offset, void* data)
{
    cache* newCache = (cache*)malloc(sizeof(cache));
    if (newCache == NULL) 
    {
        perror("Failed to allocate memory for cache");
        return; 
    }

    newCache->offset = offset;
    newCache->data = (void*)malloc(CACHE_SIZE);
    if (newCache->data == NULL) 
    {
        perror("Failed to allocate memory for cache data");
        free(newCache); 
        return; 
    }

    memcpy(newCache->data, data, CACHE_SIZE);
    newCache->dirty = 0; 

    long key = (long)(offset / CACHE_SIZE);
    createAndAddLRUNode(key, Hash);
    createAndInsertNode(root, key, (void*)newCache);
}


cache* findCache(AVLTreeNode* root, off_t offset) 
{
    AVLTreeNode* node = searchNodeByKey(root, (long)(offset / CACHE_SIZE));
    if (node == NULL) 
    {
        fprintf(stderr, "Node not found for offset %ld\n", (long)(offset / CACHE_SIZE));
        return NULL;
    }
    if (node->data == NULL) 
    {
        fprintf(stderr, "No data found in node for offset %ld\n", (long)(offset / CACHE_SIZE));
        return NULL;
    }
    return (cache*)node->data;
}

void deleteTailCache(AVLTreeNode** root, LRUHash* Hash)
{
    long key = deleteLRUTail(Hash);
    cache* cache = findCache(*root, (off_t)(key*CACHE_SIZE));
    if (cache != NULL)
    {
        if(cache->data != NULL)
        {
            free(cache->data);
            cache->data = NULL;
        }
        free(cache);
        cache = NULL;
    }
    
    deleteAndFreeNode(root, key);
}


void printAVLTreeTraversalResults(AVLTreeNode* root, LRUHash* Hash) 
{
    printf("AVL 树的中序遍历结果：\n");
    inorderTraversal(root);
    printf("\n");

    printf("AVL 树的先序遍历结果：\n");
    preorderTraversal(root);
    printf("\n");

    printLRUList(Hash);
}


/*
int main() {
    // 初始化缓存
    initializeCache();
    
   
    createCache(4096 * 0, "Data at offset");
    printAVLTreeTraversalResults();

    createCache(4096 * 1, "Data at offset");
    printAVLTreeTraversalResults();

    deleteTailCache();
    printAVLTreeTraversalResults();
    
    createCache(4096 * 0, "Data at offset");
    printAVLTreeTraversalResults();



    // 清理缓存
    printf("\n\n清理缓存：\n");
    cleanUpCache();
    printf("缓存已清理\n");

    return 0;
}


*/