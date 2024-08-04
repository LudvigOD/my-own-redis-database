#ifndef HASHTAB.H
#define HASHTAB.H

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <assert.h>


const size_t k_max_load_factor = 8;
const size_t k_resizing_work = 128;

void hm_insert(HMap *hmap, HNode *node);
HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
static void hm_start_resizing(HMap *hmap);
static void hm_help_resizing(HMap *hmap);

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

typedef struct Entry {
  HNode node;
  std::string key;
  std::string val;
} Entry;

struct {
  HMap db;
} g_data;


static void h_init(HTab *htab, size_t n){
  assert(n > 0 && ((n & n-1) == 0)); // must be able to factorize to only twos
  htab->tab = (HNode **)calloc(sizeof(HNode *), n); // creates an array pointer 
  htab->mask = n - 1; // to perform a fast module bitwise
  htab->size = 0;
}

static void h_insert(HTab *htab, HNode *node){
  size_t pos = node->hcode & htab->mask; // get position
  HNode *next  = htab->tab[pos];  // prepending node if pos is not null
  node->next = next;  // setting node to first pos in list
  htab->tab[pos] = node;  // setting node to pos in array
  htab->size++; // incrementing size
}

static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)){
  if(!htab->tab){
    return NULL;
  }
  size_t pos = key->hcode;
  HNode **from = &htab->tab[pos]; // incoming pointer to the result
  for(HNode* cur; (cur = *from) != NULL; from = &cur->next){  // now from holds the address to the next node
    if(cur->hcode == key->hcode && eq(cur, key)){
      return from;
    }
  }
  return NULL;
}

static HNode *h_detach(HTab *htab, HNode **from){
  HNode *node = *from;
  *from = node->next; // remove that node and bind the chain together
  htab->size--;
  return node;
}

void hm_insert(HMap *hmap, HNode *node){
  if(!hmap->ht1.tab){
    h_init(&hmap->ht1, 4); // initialize the table if none existing
  }
  h_insert(&hmap->ht1, node); // insert key into the table

  if(!hmap->ht2.tab){ // check load factor
    size_t load_factor = hmap->ht1.size / (hmap->ht1.mask + 1); // mask + 1 = n and size is how many nodes
    if(load_factor >= k_max_load_factor){
      hm_start_resizing(hmap);  // create a larger table
    }
  }
  hm_help_resizing(hmap); // balance hmap
}

// 
static void hm_start_resizing(HMap *hmap){
  assert(hmap->ht2.tab == NULL);
  // create a bigger hashtable and swap them
  hmap->ht2 = hmap->ht1;
  h_init(&hmap->ht1, (hmap->ht1.mask + 1) * 2);
  hmap->resizing_pos = 0;
}


// moves over nodes from ht2 to ht1 and frees ht2 if finsihed
static void hm_help_resizing(HMap *hmap){
  size_t nwork = 0;
  while(nwork < k_resizing_work && hmap->ht2.size > 0){
    // scan for nodes in ht2 and add them to ht1
    HNode **from = &hmap->ht2.tab[hmap->resizing_pos];
    if(!*from){
      hmap->resizing_pos++;
      continue;
    }
    h_insert(&hmap->ht1, h_detach(&hmap->ht2, from));
    nwork++;
  }
  if(hmap->ht2.size == 0 && hmap->ht2.tab){
    // done
    free(hmap->ht2.tab);
    hmap->ht2 = HTab{};
  }
}

// returns node if exists
HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)){

  hm_help_resizing(hmap);
  HNode **from = h_lookup(&hmap->ht1, key, eq);
  from = from ? from : h_lookup(&hmap->ht2, key, eq);
  return from ? *from : NULL;
}


// removes node from hmap
HNode *hm_pop(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)){
  hm_help_resizing(hmap);

  // checks hashtable 1
  if(HNode **from = h_lookup(&hmap -> ht1, key, eq)){
    return h_detach(&hmap->ht1, from);
  }
  // checks hashtable 2
  if (HNode **from = h_lookup(&hmap->ht2, key, eq)) {
        return h_detach(&hmap->ht2, from);
  }

  return NULL;
}

size_t hm_size(HMap *hmap){
  return (hmap->ht1.size + hmap->ht2.size);
}

void hm_destroy(HMap *hmap){
  free(&hmap->ht1);
  free(&hmap->ht2);
  *hmap = HMap{}; // sets defualt values
}

#endif