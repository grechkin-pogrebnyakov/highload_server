#ifndef _SERVER_H
#define _SERVER_H

#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include "main.h"

int main_loop( struct sockaddr_in *sockaddr );

#endif
