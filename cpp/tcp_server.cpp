#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <poll.h>

#include <assert.h>

#include "Conn.h"
#include "global_func.h"
#include "res_commands.h"
#include "hashtab.h"



enum {
  ERR_UNKNOWN = 1,
  ERR_2BIG = 2,
};

// Set descriptor to non-blocking
static void fd_set_nb(int fd);

// Add a connection to the vector mapping
static void conn_put(std::vector<Conn *> &fd2conn, Conn *conn);

// Accept a new connection
static int32_t accept_new_conn(std::vector<Conn *> &fd2conn, int fd);

// Handle the IO for a given connection state
static void connection_io(Conn *conn);

// State handlers for request and response phases
static void state_req(Conn *conn);
static void state_res(Conn *conn);

// Try to fill the buffer with incoming data
static bool try_fill_buffer(Conn *conn);

// Try to flush the buffer with outgoing data
static bool try_flush_buffer(Conn *conn);

// Try to process one request from the buffer
static bool try_one_request(Conn *conn);

static int32_t do_request(std::vector<std::string> &cmd,std::string &out);

static int32_t parse_req(const uint8_t *data, size_t len, std::vector<std::string> &out);

static bool cmd_is(const std::string &word, const char *cmd);


const size_t k_max_args = 1024;

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

static bool cmd_is(const std::string &word, const char *cmd) {
    return 0 == strcasecmp(word.c_str(), cmd);
}

static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn){
   // converting the integer of conn->fd to size_t and if that is bigger 
   // than fd2conn.size() => we need to increase size so fd has its key (index)
  if(fd2conn.size() <= (size_t)conn->fd){
    fd2conn.resize(conn->fd + 1); // +1 due to index start at 0
  }
  // set new conn to its fd key
  fd2conn[conn->fd] = conn;
}

static int32_t accept_new_conn(std::vector <Conn *> &fd2conn, int fd){
  // accept new connection
  struct sockaddr_in client_addr = {};
  socklen_t socklen = sizeof(client_addr);
  int connfd = accept(fd, (struct sockaddr *) &client_addr, &socklen);
  if (connfd < 0){
    msg("accept() error");
    return -1; // error
  }

  // set new fd to nonblocking mode
  fd_set_nb(connfd);
  // creating the struct Conn
  struct Conn *conn = (struct Conn *) malloc(sizeof(struct Conn));
  if(!conn){
    close(connfd);
    return -1;
  }

  // configuring and adding connection to fd2conn
  conn->fd = connfd;
  conn->state = STATE_REQ;
  conn->rbuf_size = 0;
  conn->wbuf_size = 0;
  conn->wbuf_sent = 0;
  conn_put(fd2conn, conn);

  return 0;
}

static int32_t do_request(std::vector<std::string> &cmd, std::string &out){
    printf("\n doing request \n");
    for(size_t i = 0; i < cmd.size(); i++){
      printf("cmd %d: %s\n", i, cmd[i].c_str());
    }
    printf("cmd size: %d \n", cmd.size());
    printf("cmd: %.*s \n", 3, cmd[0].c_str());

    if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
        printf("do_get");
        do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        printf("do_set");
        do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        printf("do_del");
        do_del(cmd, out);
    } else if (cmd.size() == 1 && cmd_is(cmd[0], "keys")){
        printf("do_keys");
        do_keys(cmd, out); //TODO
    } else {
        out_err(out, ERR_UNKNOWN, "Unknown command");
    }
    return 0;
}

static int32_t parse_req(
    const uint8_t *data, size_t len, std::vector<std::string> &out)
{
    if (len < 4) {
        return -1;
    }
    uint32_t n = 0;
    memcpy(&n, &data[0], 4);
    if (n > k_max_args) {
        return -1;
    }

    size_t pos = 4;
    while (n--) {
        if (pos + 4 > len) {
            return -1;
        }
        uint32_t sz = 0;
        memcpy(&sz, &data[pos], 4);
        if (pos + 4 + sz > len) {
            return -1;
        }
        out.push_back(std::string((char *)&data[pos + 4], sz));
        pos += 4 + sz;
    }

    if (pos != len) {
        return -1;  // trailing garbage
    }
    return 0;
}



static bool try_one_request(Conn *conn) {
    // try to parse a request from the buffer
    if (conn->rbuf_size < 4) {
        // not enough data in the buffer. Will retry in the next iteration
        printf("rbuf size: %d\n", conn->rbuf_size);
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[0], 4);
    if (len > k_max_msg) {
        msg("too long");
        conn->state = STATE_END;
        return false;
    }
    if (4 + len > conn->rbuf_size) {
        // not enough data in the buffer. Will retry in the next iteration
        printf("rbuf size: %d, is smaller than %d\n ", conn->rbuf_size, 4 + len);
        return false;
    }

    // parse the request
    std::vector<std::string> cmd;
    if (0 != parse_req(&conn->rbuf[4], len, cmd)) {
        msg("bad req");
        conn->state = STATE_END;
        return false;
    }

    // got one request, generate the response.
    std::string out;
    do_request(cmd, out);

    // pack the response into the buffer
    if (4 + out.size() > k_max_msg) {
        out.clear();
        out_err(out, ERR_2BIG, "response is too big");
    }
    uint32_t wlen = (uint32_t)out.size();
    memcpy(&conn->wbuf[0], &wlen, 4);
    memcpy(&conn->wbuf[4], out.data(), out.size());
    conn->wbuf_size = 4 + wlen;

    // remove the request from the buffer.
    // note: frequent memmove is inefficient.
    // note: need better handling for production code.
    size_t remain = conn->rbuf_size - 4 - len;
    if (remain) {
        memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_size = remain;

    // change state
    conn->state = STATE_RES;
    state_res(conn);

    // continue the outer loop if the request was fully processed
    return (conn->state == STATE_REQ);
}

static bool try_fill_buffer(Conn *conn) {
    // try to fill the buffer
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv = 0;
    do {
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
        printf("rv in fill buffer loop: %d \n", rv);
    } while (rv < 0 && errno == EINTR);
    if (rv < 0 && errno == EAGAIN) {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0) {
        msg("read() error");
        conn->state = STATE_END;
        return false;
    }
    if (rv == 0) {
        if (conn->rbuf_size > 0) {
            msg("unexpected EOF");
        } else {
            msg("EOF");
        }
        conn->state = STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t)rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));

    // Try to process requests one by one.
    printf("trying one request, rv: %zd \n", rv);
    while (try_one_request(conn)) {}
    return (conn->state == STATE_REQ);
}

static void state_req(Conn *conn){
    while(try_fill_buffer(conn)){}
}

static void state_res(Conn *conn){
    while(try_flush_buffer(conn)){}
}

static void connection_io(Conn *conn){
    if (conn->state == STATE_REQ){
        state_req(conn);
    }
    else if (conn->state == STATE_RES){
        state_res(conn);
    }
    else{
        assert(0); // not expected
    }
}

static bool try_flush_buffer(Conn *conn){
    ssize_t rv = 0;
    do {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
    } while(rv < 0 && errno == EINTR);

    if(rv < 0 && errno == EAGAIN){
        // got EAGAIN, stop
        return false;
    }

    if(rv < 0){
        msg("write() error");
        conn->state = STATE_END;
    }

    conn->wbuf_sent += (size_t) rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);
    if(conn->wbuf_sent == conn->wbuf_size){
        // respons was fully sent, change state back
        conn->state = STATE_REQ;
        conn->wbuf_size = 0;
        conn->wbuf_sent = 0;
        return false;
    }

    // still has some data in wbuf, could try to write again
    return true;
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
            // destroy this connection
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
