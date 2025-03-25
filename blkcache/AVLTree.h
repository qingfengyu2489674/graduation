#ifndef AVL_TREE_H
#define AVL_TREE_H


#define HEIGHT(N) ((N) == NULL ? 0 : (N)->height)
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define GET_BALANCE(N) (HEIGHT((N)->left) - HEIGHT((N)->right))

typedef struct AVLTreeNode
{
	struct AVLTreeNode *left;
	struct AVLTreeNode *right;
	int height;
	long key;
	void *data;
}AVLTreeNode;


void createAndInsertNode(AVLTreeNode** root, long key, void *data);
AVLTreeNode* searchNodeByKey(AVLTreeNode* root, long key);
void deleteAndFreeNode(AVLTreeNode** root, long key);
void cleanUpAVLTree(AVLTreeNode** root);

void inorderTraversal(AVLTreeNode* root);
void preorderTraversal(AVLTreeNode* root);

#endif