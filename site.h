#ifndef SITE_H
#define SITE_H

#define PORT 8080
#define MAX_CONNECTIONS 5
#define BUFFER_SIZE 8192

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "export.h"

typedef struct Header {
  char version[BUFFER_SIZE];
  char method[BUFFER_SIZE];
  char path[BUFFER_SIZE];
} Header;

void* thread_fn(void* arg);

#endif