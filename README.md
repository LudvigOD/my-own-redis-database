# This is my redis database
## Purpose
* Understanding how TCP servers work and how a single thread database like Redis is implemented
* I have also created a TCP server with a HTTP protocol (not published yet), which I am going to use later with my own implementation of React.




## My own notes about functions and how the program works

### Conn.h
* It is basically a client file descriptor but with other attributes to monitor the clients and decide what to do based on the STATE value. It includes an enum with diffrent states that decides what operation is to be made.
### global_funcs.h
* Stores global varaiables
* read_full(int fd, char *buf, size_t n): Reads n bytes from res req and stores it in a buffer. Apperently if a peer/client disconnect EOF occurs, by using a while loop connection can restore. Finally we move cursor up in buffer to read remaining data.
*write_all(int fd, char *buf, size_t n): Does pretty much the same as read but sends information instead.
### tcp_server.cpp
* Protocol: |nr of str| |length1| |str1| |length2| |str2| |length3| |str3| ...
* * The command in this design is a list of strings i.e set key val
* Respons protocol: |res| |data|  // res is a status code and data is the respons string 
 
* fd_set_nb(int fd): incorperate non-blocking flag to existing flags.

* conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn): adds connection (conn) to vector fd2conn (which acts like a map with fd integer as key) and resizes it if key is index out of bounds.

* accept_new_conn(): accepts a connected client socket and sets correct flags with fd_set_nb (nb = non-blocking). Then it creates a Conn instance with default values which is put in the fd2conn.

* try_fill_buffer(Conn *conn): Calculates how many bytes can be read into rbuf and tries to read from connected fd (conn->fd), if errno = EINTR try again, and handles other errors like EAGAIN => return false and exits the while loop. If EOF => change state to END returns false. Updates rbuf_size. Then while loop try_one_request, PIPELINING. Finally returns true if STATE = request otherwise false. 

* try_flush_buffer(Conn *conn): It calculates the remaining data size of what has not been sent (write()) from write buffer (wbuf). If it is the defualt value, it's zero. Otherwize it writes the remaining and starts in wbuf at index wbuf_sent which is the size of sent data. If EAGAIN, stop loop return false. If written data is zero => STATE = end. Update wbuf_sent. If wbuf_sent == wbuf_size then all data was sent and STATE = request and conn's default values are set, break loop by returning false. Else, wbuf_sent != wbuf_size return true to write again.

* try_one_request(Conn *conn): It uses the conn to access read buffer and if data is formated and acts according to protocol it reads length1 amount of bytes after 4 bytes (the size of length) and then copies that data to write buffer for later. If not, then it returns false and exits the while loop it is called in. Then it moves the "unread" remaining data to the beginning of read buffer (incl. length2 and data2) and changes size of the rbuf. STATE changes to RESPOND. Returns false.

* state_req(Conn *conn): Just a while loop without body which calls try_fill_buffer. Reading requests from client.

* state_res(Conn *conn): Just a while loop calliing try_flush_buffer. Write responds to client.

* connection_io(Conn *conn): Decides whether to call state_req, state_res or exit program, depening on connection's STATE.

* parse_req(const uint8_t *data, size_t len, std::vector<std::string> &out): For loops over the amount of strings in req and stores every string in the data in the out variable's address. There is a "pos" variable in the for loop to keep track of what has been read. Finally, if len > pos there is trailing garbage and function return -1 error.

* do_request(const uint8_t *req, uint32_t reqlen, uint32_t *rescode, uint8_t *res, uint32_t *reslen):
This function uses the parse_req() to parse the req and creates a vector consisting of strings which is names cmd. We check what the first string is in cmd and decide which response command is to be made (get, set, del) and uses respctive function for that cmd which decides the rescode.  If the cmd is unkown, throw error. 