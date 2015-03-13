#ifndef _MAIN_H
#define _MAIN_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "server.h"
#include <unistd.h>

int16_t log_level;
uint16_t is_root;
pid_t pid;

#endif
