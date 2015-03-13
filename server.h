#ifndef _SERVER_H
#define _SERVER_H

#include <stdio.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
#include "main.h"
#include <time.h>
#include <string.h>
#include <ctype.h>

uint32_t conn_id;

typedef struct conn_t {\
    uint32_t id;
    uint16_t read_finished;
    uint16_t write_finished;
    uint16_t data_ready;
    uint16_t read_url;
    char *method;
    char *addr;
    char *request;
    uint32_t status;
    int data;
    size_t data_len;
    char *content_type;
//    char *vary;
} conn_t;

int main_loop( int socketfd );

#endif
