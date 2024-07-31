#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <vector>

#include "global_func.h"

static int32_t send_req(int fd, const std::vector<std::string> &cmd){
    uint32_t len = 4;
    for(const std::string &s : cmd){
        len += 4 + s.size();
    }
    if(len > k_max_msg){
        return -1;
    }


    char wbuf[4 + k_max_msg];
    memcpy(&wbuf[0], &len, 4);
    uint32_t n = cmd.size();
    memcpy(&wbuf[4], &n, 4);
    size_t cur = 8;
    for(const std::string &s : cmd){
        uint32_t p = (uint32_t) s.size();
        memcpy(&wbuf[cur], &p, 4);  // write at 8th pos because 0-3 is the whole wbuf len and 
                                    // 4-7 is the whole cmd len, and 8-p is the diffrent args in cmd 
        memcpy(&wbuf[cur + 4], s.data(), s.size()); 
        cur += 4 + s.size();
    }


    return(write_all(fd, wbuf, 4 + len));   // 4 + len bc it doesnt account for itself
}

static int32_t read_res(int fd){
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if(err){
        if (errno == 0){
            msg("EOF");
        }
        else{
            msg("read() error");
        }

        return err;
    }

    uint32_t len = 0;
    memcpy(&len, &rbuf, 4);

    if(len > k_max_msg){
        msg("too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);
    if(err){
        msg("read() error");
        return err;
    }

    uint32_t rescode = 0;
    if(len < 4){
        msg("bad response");
        return -1;
    }

    memcpy(&rescode, &rbuf[4], 4);

    // do something
    printf("Server says: [%u] %.*s\n", rescode, len - 4, &rbuf[8]);
    return 0;
}


uint32_t query(int fd, const char *text){
    uint32_t len = (uint32_t) strlen(text);
    if(len > k_max_msg){
        return -1;
    }

    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    if(int32_t err = write_all(fd, wbuf, 4 + len)){
        return err;
    }

    // 4 bytes header
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if(err){
        if (errno == 0){
            msg("EOF");
        }
        else{
            msg("read() error");
        }

        return err;
    }

    memcpy(&len, rbuf, 4);

    if(len > k_max_msg){
        msg("too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);
    if(err){
        msg("read() error");
        return err;
    }

    // do something
    rbuf[4 + len] = '\0';
    printf("Server says: %s\n", &rbuf[4]);
    return 0;
}


int main(int argc, char **argv){

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        die("socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(PORT);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    
    // connecting socket to address
    int rv = connect(fd, (struct sockaddr *) &addr, sizeof(addr));

    if(rv){
        die("connect()");
    }

    std::vector<std::string> cmd;

    for(int i = 1; i < argc; i++){
        cmd.push_back(argv[i]);
    } 

    int32_t err = send_req(fd, cmd);
    if(err){
        goto L_DONE;
    }

    err = read_res(fd);
    if(err){
        goto L_DONE;
    }

L_DONE:
    close(fd);
    return 0;
}