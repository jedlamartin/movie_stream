#ifndef SITE_H
#define SITE_H

#define PORT 8080
#define MAX_CONNECTIONS 5
#define BUFFER_SIZE 8192
#define BODY_MAX_SIZE 1048576 // 1 MB

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
#include <stdbool.h>

#include "export.h"
#include "list.h"

typedef struct Header {
  char version[BUFFER_SIZE];
  char method[BUFFER_SIZE];
  char path[BUFFER_SIZE];
  bool keep_alive;
  bool range_request;
  size_t range_start;
  size_t range_end;
  List* headers;
} Header;

void* thread_fn(void* arg);

#endif