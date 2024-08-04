#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <vector>
#include <sys/types.h>
#include <assert.h>
#include "global_func.h"
#include "hashtab.h"


#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) );}) // gets the starting bytes for the structure



enum {
  RES_OK = 0,
  RES_ERR = 1,
  RES_NX = 2,
};

std::map<std::string, std::string> g_map;

// this hashing is called FNV1, but with slight modification
uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        // adding ith element from data to h and multilying the sum with a hexdec
        h = (h + data[i]) * 0x01000193; 
    }
    return h;
}


bool entry_eq(HNode *lhs, HNode *rhs){
  Entry *le = container_of(lhs, Entry, node);
  Entry *re = container_of(rhs, Entry, node);
  return le->key == re->key;
}

uint32_t do_get(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen){
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *) key.key.data(), key.key.size());

  HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
  if(!node){
    return RES_NX;
  }

  const std::string &val = container_of(node, Entry, node)->val;
  assert(val.size() <= k_max_msg);
  memcpy(res, val.data(), val.size());
  *reslen = (uint32_t) val.size();

  return RES_OK;
}

uint32_t do_set(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen){
  (void)res;
  (void)reslen;
  
  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

  HNode *node = hm_lookup(&g_data.db, &key.node, entry_eq);

  if(node){
    container_of(node, Entry, node)->val.swap(cmd[2]);
  } 
  // creating a new entry and adding its node to hmap
  else{
    Entry *ent = new Entry();
    ent->key.swap(key.key);
    ent->node.hcode = key.node.hcode;
    ent->val.swap(cmd[2]);
    hm_insert(&g_data.db, &ent->node);
  }

  return RES_OK;
}

uint32_t do_del(std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen){
  (void)res;
  (void)reslen;

  Entry key;
  key.key.swap(cmd[1]);
  key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

  HNode *node = hm_pop(&g_data.db, &key.node, &entry_eq);
  if(node){
    delete container_of(node, Entry, node);
  }

  return RES_OK;
}
