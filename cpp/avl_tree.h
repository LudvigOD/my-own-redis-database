#ifndef AVL_TREE_H
#define AVL_TREE_H

#include <sys/types.h>
#include <stdlib.h>
#include <set>
#include <assert.h>


typedef struct AVLNode {
  uint64_t depth; // subtree height
  uint64_t cnt;   // subtree size
   // since AVLNode alias is not totally finished, it's safer to use struct (in C, not C++)
  struct AVLNode *left;
  struct AVLNode *right;
  struct AVLNode *parent;
} AVLNode;

typedef struct Data {
  AVLNode node;
  uint32_t val = 0;
} Data;

typedef struct Containter {
  AVLNode *root = NULL;
} Container;

void avl_init(AVLNode *node);
uint64_t avl_depth(AVLNode *node);
uint64_t avl_cnt(AVLNode *node);
void avl_update(AVLNode *node);
AVLNode *rot_left(AVLNode *node);
AVLNode *rot_right(AVLNode *node);
// left subtree is too deep
AVLNode *avl_fix_left(AVLNode *node);
AVLNode *avl_fix_right(AVLNode *node);
// fix imbalances and maintain invariants until the root is reached
AVLNode *avl_fix(AVLNode *node);
// only detach a node an returns the new root of the tree
AVLNode *avl_del(AVLNode *node);
// add a Data with val
void add(Container &c, uint32_t val);
// delete a Data that matches val
bool del(Container &c, uint32_t val);
// verify node
void avl_verify(AVLNode *parent, AVLNode *node);
// extract all values from subtree node
void extract(AVLNode *node, std::multiset<uint32_t> &extracted);
// verify the Container with the values
void container_verify(Container &c, const std::multiset<uint32_t> &ref);
// removing all data
void dispose(Container &c);
#endif