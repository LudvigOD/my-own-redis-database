#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include "Conn.cpp"
#include <vector>
#include <poll.h>

#include <assert.h>

#include "global_func.cpp"



// static void do_something(int connfd){
//   char rbuf[64] = {};
//   ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);

//   if(n < 0){
//     msg("read() error");
//     return;
//   }

//   printf("client says: %s\n", rbuf);

//   char wbuf[] = "world";
//   write(connfd, &wbuf, sizeof(wbuf));
// }

static int32_t one_request(int connfd){

  //4 bytes header
  char rbuf[4 + k_max_msg + 1];
  errno = 0;
  int32_t err = read_full(connfd, rbuf, 4);
  if(err){
    if(errno == 0){
      msg("EOF");
    }
    else {
      msg("read() error");
    }

    return err;
  }

  uint32_t len = 0;
  memcpy(&len, rbuf, 4); // copy the 4 bytes symbolizing the length to &len

  if(len > k_max_msg){
    msg("too long");
    return -1;
  }

  // req body
  err = read_full(connfd, &rbuf[4], len); // reading the rest and putting it at the fifth position (bytes)

  if(err){
    msg("read() error");
    return err;
  }

  // do somet
  rbuf[4 + len] = '\0';
  printf("client says: %s\n", &rbuf[4]);

  // reply using the same protocol
  const char reply[] = "world";
  char wbuf[4 + sizeof(reply)];
  len = (uint32_t) strlen(reply);
  memcpy(wbuf, &len, 4);
  memcpy(&wbuf[4], reply, len);

  return write_all(connfd, wbuf, 4 + len);
}

// set descriptor to non-blocking
static void fd_set_nb(int fd){
  errno = 0;
  int flags = fcntl(fd, F_GETFL, 0); // getting flags from the fd
  if(errno){
    die("fcntl() error");
    return;
  }
  flags |= O_NONBLOCK; // combines the prev flags with O_NONBLOCK (very weird)

  errno = 0;
  (void) fcntl(fd, F_SETFL, flags); // making it so it doesnt return anything (not neccessary)
  if(errno){
    die("fcntl() error");
  }

}


static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn){
   // converting the integer of conn->fd to size_t and if that is bigger 
   // than fd2conn.size() => we need to increase size so fd has its key (index)
  if(fd2conn.size() <= (size_t)conn->fd){
    fd2conn.resize(conn->fd + 1);
  }
  fd2conn[conn->fd] = conn;
}

int main(){
       
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
      die("socket()");
    }
    
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(PORT);
    addr.sin_addr.s_addr = ntohl(0);

    int rv = bind(fd, (const sockaddr*) &addr, sizeof(addr));
    if(rv) {
      die("bind()");
    }

    rv = listen(fd, SOMAXCONN);
    if(rv){
      die("listen()");
    }

    // a map of all client connections, keyed by fd (this is a vector => will auto increase its size)
    std::vector<Conn *> fd2conn;

    // set the listen fd to nonblocking mode
    fd_set_nb(fd);
    
    // event loop
    std::vector<struct pollfd> poll_args;
    while(true){
      // prepare arguments for poll
      poll_args.clear();
      // for convenience, the listening fd is put to the first position
      struct pollfd pfd = {fd, POLLIN, 0};
      poll_args.push_back(pfd);
      // connection fds
      for (Conn *conn : fd2conn){ 
        if(!conn){ // checking if fd is active
          continue;
        }
        struct pollfd pfd = {};
        pfd.fd = conn->fd;
        pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
        pfd.events = pfd.events | POLLERR; // deciding what events to be monitored (POLLIN or POLLOUT and POLLERR)
        poll_args.push_back(pfd);
      }

      // poll for active fds
      // timeout arg doesnt matter here
      int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000); // (converting to an array, using size method for size of array, timeout)
      if (rv < 0){
        die("poll()");
      }


      // process active connections
      for(int i = 1; i < poll_args.size(); i++){
        if(poll_args[i].revents){
          Conn *conn = fd2conn[poll_args[i].fd]; // grab unique integer from fd and use it as a key for that index
          connection_io(conn);
          if (conn->state == STATE_END) {
            // client close normally, or something bad happens
            // destroy thsi connection
            fd2conn[conn->fd] = NULL;
            (void) close(conn->fd);
            free(conn);
          }
        }
      }
      // try to accept a new connection if the listening fd is active (index 0 is set to POLLIN on every iteration of while loop)
      if(poll_args[0].revents){
        (void) accept_new_conn(fd2conn, fd);
      }

    }


    
    return 0;
}
