#include <stdlib.h>
#include <stdio.h>


int PORT = 1234;
const size_t k_max_msg = 4096;

// read all data sent by client
static int32_t read_full(int fd, char *buf, size_t n){
  while (n>0){
    ssize_t rv = read(fd, buf, n);

    if(rv <= 0){
      return -1; //error, unexpected EOF
    }

    assert((size_t) rv <= n); // controll check
    n -= (size_t) rv; // update how many bytes are left to read
    buf += rv; // move cursor (bytes) to the unread part
  }

  return 0;
}


// write all data to client
static int32_t write_all(int fd, char *buf, size_t n){

  while(n > 0){
    ssize_t rv = write(fd, buf, n);

    if(rv <= 0){
      return -1; // error, unexpected EOF
    }
    assert((size_t) rv <= n);
    n -= (size_t) rv;
    buf += rv;
  }

  return 0;
}

void die(const char* msg){
  perror(msg);
  exit(EXIT_FAILURE);
}

void msg(const char* msg){
  perror(msg);
}