#include "hashmap.h"

const size_t k_max_load_factor = 8;
const size_t k_resizing_work = 128;


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

void hm_insert(HMap *hmap, HNode *node) {
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

static void hm_start_resizing(HMap *hmap){
  assert(hmap->ht2.tab == NULL);
  // create a bigger hashtable and swap them
  hmap->ht2 = hmap->ht1;
  h_init(&hmap->ht1, (hmap->ht1.mask + 1) * 2);
  hmap->resizing_pos = 0;
}

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


