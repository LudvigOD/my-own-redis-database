#include <sys/types.h>
#include "global_func.cpp"

enum {
  STATE_REQ = 0,
  STATE_RES = 1,
  STATE_END = 2,
};

struct Conn
{
  /* data */
  int fd = -1;
  uint32_t state = 0; //either SATE_REQ or STATE_RES
  // buffer for reading
  size_t rbuf_size = 0;
  uint8_t rbuf[4 + k_max_msg];
  // buffer for writing
  size_t wbuf_size = 0;
  size_t wbuf_sent = 0;
  uint8_t wbuf[4 + k_max_msg]; 

};
