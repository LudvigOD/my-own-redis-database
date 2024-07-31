#ifndef HASHMAP.H
#define HASHMAP.H
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// hashtable interface
typedef struct HMap {
  HTab ht1;  // Newer
  HTab ht2;  // Older
  size_t resizing_pos = 0;

} HMap;

typedef struct HNode {
  HNode *next = NULL;
  uint32_t hcode = 0;
} HNode;

typedef struct HTab {
  HNode **tab = NULL;
  size_t mask = 0;
  size_t size = 0;
} HTab;

void hm_insert(HMap *hmap, HNode *node);


#endif