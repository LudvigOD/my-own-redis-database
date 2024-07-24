#pragma once
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <vector>
#include <sys/types.h>
#include <assert.h>
#include "global_func.h"


enum {
  RES_OK = 0,
  RES_ERR = 1,
  RES_NX = 2,
};

std::map<std::string, std::string> g_map;

uint32_t do_get(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen){
  if(!g_map.count(cmd[1])){
    return RES_NX;
  }
  std::string &val = g_map[cmd[1]];
  assert(val.size() <= k_max_msg);
  memcpy(res, val.data(), val.size());
  *reslen = (uint32_t) val.size();

  return RES_OK;
}

uint32_t do_set(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen){
  (void)res;
  (void)reslen;
  g_map[cmd[1]] = cmd[2];

  return RES_OK;
}

uint32_t do_del(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen){
  (void)res;
  (void)reslen;
  g_map.erase(cmd[1]);

  return RES_OK;
}
