#include <stdio.h> 
#include <stdlib.h>
#include <stdbool.h>

#include "AVLTree.h"


AVLTreeNode* searchNodeByKey(AVLTreeNode* root, long key);
void createAndInsertNode(AVLTreeNode** root, long key, void *data);
void deleteAndFreeNode(AVLTreeNode** root, long key);
void cleanUpAVLTree(AVLTreeNode** root);
void inorderTraversal(AVLTreeNode* root);
void preorderTraversal(AVLTreeNode* root); 



AVLTreeNode* right_rotate(AVLTreeNode* y) 
{
    AVLTreeNode* x = y->left;
    AVLTreeNode* T2 = x->right;

    x->right = y;
    y->left = T2;

    y->height = MAX(HEIGHT(y->left), HEIGHT(y->right)) + 1;
    x->height = MAX(HEIGHT(x->left), HEIGHT(x->right)) + 1;

    return x;
}

AVLTreeNode* left_rotate(AVLTreeNode* x) 
{
    AVLTreeNode* y = x->right;
    AVLTreeNode* T2 = y->left;

    y->left = x;
    x->right = T2;

    x->height = MAX(HEIGHT(x->left), HEIGHT(x->right)) + 1;
    y->height = MAX(HEIGHT(y->left), HEIGHT(y->right)) + 1;

    return y;
}

AVLTreeNode* balanceTree(AVLTreeNode* root) 
{
    root->height = 1 + MAX(HEIGHT(root->left), HEIGHT(root->right));
    int balance = GET_BALANCE(root);

    if (balance > 1 && GET_BALANCE(root->left) >= 0) 
    {
        return right_rotate(root);
    }

    if (balance > 1 && GET_BALANCE(root->left) < 0) 
    {
        root->left = left_rotate(root->left);
        return right_rotate(root);
    }

    if (balance < -1 && GET_BALANCE(root->right) <= 0) 
    {
        return left_rotate(root);
    }

    if (balance < -1 && GET_BALANCE(root->right) > 0) 
    {
        root->right = right_rotate(root->right);
            return left_rotate(root);
        }

    return root;
}


AVLTreeNode* insertNodeByKey(AVLTreeNode* root, AVLTreeNode* node)
{
	if (root == NULL)
	{
		return node;
	}

	if(node->key < root->key)
	{
		root->left = insertNodeByKey(root->left, node);
	}


	else if(node->key > root->key)
	{
		root->right = insertNodeByKey(root->right, node);
	}

	else
    {
        return root;
    }

	return balanceTree(root);
	
}


AVLTreeNode* searchNodeByKey(AVLTreeNode* root, long key) 
{
    if (root == NULL || root->key == key) 
	{
        return root;
    }

    if (key < root->key) 
	{
        return searchNodeByKey(root->left, key);
    }

    return searchNodeByKey(root->right, key);
}


AVLTreeNode* deleteNode(AVLTreeNode* root, AVLTreeNode** deletedNode) 
{
    if (deletedNode == NULL || *deletedNode == NULL || root == NULL) 
	{
        return root;
    }

    if ((*deletedNode)->key < root->key)
	{
        root->left = deleteNode(root->left, deletedNode);
    } 

	else if ((*deletedNode)->key > root->key) 
	{
        root->right = deleteNode(root->right, deletedNode);
    } 
	else 
	{
        if (root->left == NULL || root->right == NULL) 
		{
            AVLTreeNode *temp = root->left ? root->left : root->right;
            if (temp == NULL) 
			{
                temp = root;
                root = NULL;
            } 
			else 
			{  
                //root->key = temp->key;
                //root->data = temp->data;
                *root = *temp;
            }

            *deletedNode = temp; 

        } 
		else 
		{
            AVLTreeNode* temp = root->right;
            while (temp->left != NULL) 
			{
                temp = temp->left;
            }
            root->key = temp->key;
            root->data = temp->data;
            root->right = deleteNode(root->right, &temp);
            *deletedNode = temp; 
        }
    }

    if (root == NULL) 
	{
        return root;
    }

    return balanceTree(root);

}


AVLTreeNode* createNode(long key, void *data) 
{
    AVLTreeNode* node = (AVLTreeNode*)malloc(sizeof(AVLTreeNode));
    node->key = key;
    node->data = data;
    node->left = node->right = NULL;
    node->height = 1; 
    return node;
}

void createAndInsertNode(AVLTreeNode** root, long key, void *data) 
{
    AVLTreeNode* newNode = createNode(key, data);
    *root = insertNodeByKey(*root, newNode);
}


void deleteAndFreeNode(AVLTreeNode** root, long key) 
{
    if (root == NULL || *root == NULL) {
        fprintf(stderr, "Error: Tree root is NULL\n");
        return;
    }

    AVLTreeNode* nodeToDelete = searchNodeByKey(*root, key);
    if (nodeToDelete == NULL) {
        fprintf(stderr, "Error: Node with key %ld not found\n", key);
        return;
    }

    printf("删除的节点：%ld\n", nodeToDelete->key);

    *root = deleteNode(*root, &nodeToDelete);

    if (nodeToDelete != NULL) 
    {
        free(nodeToDelete);
        nodeToDelete = NULL;
    }
}
/*
void cleanUpAVLTree(AVLTreeNode** root) 
{
    while (*root != NULL || root != NULL) 
    {
        deleteAndFreeNode(root, (*root)->key);
    }
    *root = NULL; 
}
*/

void cleanUpAVLTree(AVLTreeNode** root) 
{
    if (root == NULL || *root == NULL) 
    {
        return;
    }

    cleanUpAVLTree(&((*root)->left));

    cleanUpAVLTree(&((*root)->right));

    if ((*root)->data != NULL) 
    {
         printf("deleteNode key: %ld, height: %d, left: %p, right: %p, data: %s\n", 
               (*root)->key, 
               (*root)->height, 
               (void*)(*root)->left, 
               (void*)(*root)->right, 
               (char*)(*root)->data);
        if((*root)->data != NULL)
        {
            free((*root)->data);
        }
        (*root)->data = NULL;
    }


    free(*root);
    *root = NULL; 
}

void inorderTraversal(AVLTreeNode* root)
 {
    if (root != NULL) {
        inorderTraversal(root->left);
        
        printf("Node key: %ld, height: %d, left: %p, right: %p, data: %s\n", 
               root->key, 
               root->height, 
               (void*)root->left, 
               (void*)root->right, 
               (char*)root->data);
        inorderTraversal(root->right);
    }
}

void preorderTraversal(AVLTreeNode* root) 
{
    if (root != NULL) {
        printf("Node key: %ld, height: %d, left: %p, right: %p, data: %s\n", 
               root->key, 
               root->height, 
               (void*)root->left, 
               (void*)root->right, 
               (char*)root->data);
        preorderTraversal(root->left);
        preorderTraversal(root->right);
    }
}

/*

int main() {
    AVLTreeNode* root = NULL;

    // 创建并插入节点
    createAndInsertNode(&root, 0, NULL);
    createAndInsertNode(&root, 1, NULL);  
    createAndInsertNode(&root, 6, NULL);
    createAndInsertNode(&root, 7, NULL);
    createAndInsertNode(&root, 8, NULL);  
    createAndInsertNode(&root, 9, NULL);

    // 打印树的遍历结果
    printf("初始树的中序遍历结果：\n");
    inorderTraversal(root);
    printf("\n");

    printf("初始树的先序遍历结果：\n");
    preorderTraversal(root);
    printf("\n");

    // 查找节点
    printf("查找节点：\n");
    AVLTreeNode* foundNode = searchNodeByKey(root, 8);
    if (foundNode != NULL) {
        printf("找到节点：%ld\n", foundNode->key);
    } else {
        printf("未找到节点 %ld\n", (long)8);
    }

    // 删除节点
    printf("删除节点：\n");
    //deleteAndFreeNode(&root, 8);

    // 打印删除后的遍历结果
    printf("删除节点后的中序遍历结果：\n");
    inorderTraversal(root);
    printf("\n");

    printf("删除节点后的先序遍历结果：\n");
    preorderTraversal(root);
    printf("\n");

    // 清理 AVL 树中的所有节点
    printf("清理 AVL 树中的所有节点\n");
    cleanUpAVLTree(&root);
    printf("AVL 树已被清理\n");

    return 0;
}

*/


