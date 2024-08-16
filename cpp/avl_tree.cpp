#include <algorithm>

#include "avl_tree.h"
#include "global_func.h"


void avl_init(AVLNode *node){
  node->cnt = 1;
  node->depth = 1;
  node->left = node->right = node->parent = NULL;
}

uint64_t avl_depth(AVLNode *node){
  return node ? node->depth : 0;
}

uint64_t avl_cnt(AVLNode *node){
  return node ? node->cnt : 0;
}

void avl_update(AVLNode *node){
  node->depth = 1 + std::max(avl_depth(node->right), avl_depth(node->left));
  node->cnt = 1 + avl_cnt(node->right) + avl_cnt(node->left);
}

AVLNode *rot_left(AVLNode *node){
  AVLNode *new_node = node->right; // root
  if(new_node->left){
    new_node->left->parent = node;
  }
  node->right = new_node->left; // rotation
  new_node->left = node;        // rotation
  new_node->parent = node->parent;  // since new root => new_node needs prev roots parent
  node->parent = new_node;
  avl_update(node);
  avl_update(new_node);
  return new_node;
}

AVLNode *rot_right(AVLNode *node){
  AVLNode *new_node = node->left;
  if(new_node->right){
    new_node->right->parent = node;
  }
  node->left = new_node->right;
  new_node->right = node;
  new_node->parent = node->parent;
  node->parent = new_node;
  avl_update(node);
  avl_update(new_node);
  return new_node;
}

/*
Rule 1: A right rotation restores balance if the left subtree is deeper by 2,
and the left left subtree is deeper than the left right subtree.

Rule 2: A left rotation makes the left subtree deeper than the right subtree 
while keeping the rotated height if the right subtree is deeper by 1.
*/
AVLNode *avl_fix_left(AVLNode *root){
  if(avl_depth(root->left->left) < avl_depth(root->left->right)){

    // creating an easier tree to work with which is left heavy
    root->left = rot_left(root->left); // rule 2
  }
  // balancing the left heavy tree
  return rot_right(root); // rule 1
}

AVLNode *avl_fix_right(AVLNode *root){
  if(avl_depth(root->right->right) < avl_depth(root->right->left)){
    root->right = rot_right(root->right);
  }
  return rot_left(root);
}

AVLNode *avl_fix(AVLNode *node){
  while(true) {
    avl_update(node);
    uint32_t l = avl_depth(node->left);
    uint32_t r = avl_depth(node->right);
    AVLNode **from = NULL;
    if(AVLNode *p = node->parent){
      // is crnt node left or right child? from is the temp root
      from = (p->left == node) ? &p->left : &p->right;
    }
    if(l == r + 2){
      node = avl_fix_left(node);
    }
    else if(l + 2 == r){
      node = avl_fix_right(node);
    }
    if(!from){
      // no parent => root
      return node; 
    }
    *from = node; // changing what the parent points to (the new tmp root)
    node = node->parent;
  }
}

AVLNode *avl_del(AVLNode *node){
  if(node->right == NULL){
    // no right subtree, replace the node with left subtree
    // link the left subtree to the parent
    AVLNode *parent = node->parent;
    if(node->left){
      node->left->parent = parent;
    }
    if(parent){
      // attach the left subtree to the parent, if it is not final root
      (parent->left == node ? parent->left : parent->right) = node->left;
      return avl_fix(parent); // balance new tree
    }
    else{ // removing root?
      // node is root
      return node->left;
    }
  }
  else{
    // detach the successor
    AVLNode *victim = node->right;
    while(victim->left){
      victim = victim->left;
    }
    AVLNode *root = avl_del(victim); // deleting the last victim (successor)
    // swap it with
    *victim = *node;
    if(victim->left){
      victim->left->parent = victim;
    }
    if(victim->right){
      victim->right->parent = victim;
    }

    if(AVLNode *parent = node->parent){
      (parent->left == node ? parent->left : parent->right) = victim;
      return root;
    } 
    else{ // removing root?
      return victim;
    }
  }
}

void add(Container &c, uint32_t val){
  Data *data = new Data();
  avl_init(&data->node);
  data->val = val;
  
  AVLNode *cur = NULL;
  AVLNode **from = &c.root; // incoming pointer to next node
  while(*from){             // tree search
    cur = *from;
    uint32_t node_val = container_of(cur, Data, node)->val;
    from = (val < node_val) ? &cur->left : &cur->right;
  }
  *from = &data->node;  // attaching new node to nullptr
  data->node.parent = cur;
  c.root = avl_fix(&data->node);
}

bool del(Container &c, uint32_t val){
  AVLNode *cur = c.root;
  while(cur){
    uint32_t node_val = container_of(cur, Data, node)->val;
    if(val == node_val){
      break;
    }
    cur = val < node_val ? cur->left : cur->right;
  }
  if(!cur){
    return false;
  }
  c.root = avl_del(cur);
  delete container_of(cur, Data, node); // deletes Data and its node
  return true;
}

void avl_verify(AVLNode *parent, AVLNode *node){
  if(!node){
    return;
  }
  //verify subtrees recursivly
  avl_verify(node, node->right);
  avl_verify(node, node->left);

  // 1. The parent pointer is correct.
  assert(node->parent == parent);
  // 2. The auxiliary data is correct.
  assert(node->cnt == 1 + avl_cnt(node->left) + avl_cnt(node->right));
  uint32_t l = avl_depth(node->left);
  uint32_t r = avl_depth(node->right);
  assert(node->depth == std::max(l, r) + 1);
  // 3. The height invariant is OK.
  assert(l == r || l + 1 == r || l == r +1);
  // 4. The data is ordered.
  uint32_t val = container_of(node, Data, node)->val;
  if(node->left){
    assert(node->left->parent == node);
    assert(container_of(node->left, Data, node)->val <= val);
  }
  if(node->right){
    assert(node->right->parent == node);
    assert(container_of(node->right, Data, node)->val >= val);
  }
}

void extract(AVLNode *node, std::multiset<uint32_t> &extracted){
  if(!node){
    return;
  }
  extract(node->left, extracted);
  extracted.insert(container_of(node, Data, node)->val);
  extract(node->right, extracted);
}

void container_verify(Container &c, const std::multiset<uint32_t> &ref){
  avl_verify(NULL, c.root);
  assert(avl_cnt(c.root) == ref.size());
  std::multiset<uint32_t> extracted;
  extract(c.root, extracted);
  assert(extracted == ref);
}

void dispose(Container &c){
  while(c.root){
    AVLNode *node = c.root;
    c.root = avl_del(c.root);
    delete container_of(node, Data, node);
  }
}



