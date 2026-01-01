#ifndef SITE_H
#define SITE_H

#define BUFFER_SIZE   8192       // Buffer size for reading requests/responses
#define BODY_MAX_SIZE 1048576    // Maximum allowed HTTP body size (1 MB)

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ffmpeg_utils.h"
#include "list.h"

/**
 * @struct Header
 * @brief Represents the parsed HTTP request header.
 *
 * This structure holds information extracted from an HTTP request header.
 * It includes the HTTP version, method (e.g., GET, POST), requested path,
 * connection persistence (keep-alive), range request details, and a list
 * of additional headers.
 *
 * Fields:
 * - version:      The HTTP version string (e.g., "HTTP/1.1").
 * - method:       The HTTP method string (e.g., "GET", "POST").
 * - path:         The requested resource path.
 * - keep_alive:   Indicates if the connection should be kept alive.
 * - range_request:True if the request includes a Range header.
 * - range_start:  The starting byte for a range request (if applicable).
 * - range_end:    The ending byte for a range request (if applicable).
 * - headers:      Pointer to a linked list of additional HTTP headers.
 */
typedef struct Header {
    char version[BUFFER_SIZE - 1];
    char method[BUFFER_SIZE - 1];
    char path[PATH_MAX - 1];
    bool keep_alive;
    bool range_request;
    off_t range_start;
    off_t range_end;
    List* headers;
} Header;

/**
 * @brief Thread function to handle client connections.
 *
 * This function is intended to be used as the entry point for a new thread
 * that handles a single client connection. The argument should be a pointer
 * to the client socket file descriptor or a structure containing connection
 * information. The function processes the client's HTTP request, sends the
 * appropriate response, and closes the connection when done.
 *
 * @param arg Pointer to the client connection information (typically a socket
 * fd).
 * @return void* Always returns NULL.
 */
void* thread_fn(void* arg);

#endif