#ifndef SITE_H
#define SITE_H

#define PORT 8080
#define MAX_CONNECTIONS 5
#define BUFFER_SIZE 8192

#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "export.h"

void* thread_fn(void* arg);

#endif