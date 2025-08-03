#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "export.h"
#include "site.h"

int main(int argc, char* argv[]) {
  int socket_fd = -1;
  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    char msg[] = "Could not create socket!";
    write(STDERR_FILENO, msg, sizeof(msg));
    exit(1);
  }

  // Enabling port reuse
  int opt = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
    char msg[] = "Could not set the reuse of the socket!";
    write(STDERR_FILENO, msg, sizeof(msg));
    exit(1);
  }

  // Bind
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_addr.s_addr = INADDR_ANY,
                             .sin_port = htons(PORT)};
  if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    char msg[] = "Could not bind socket!";
    write(STDERR_FILENO, msg, sizeof(msg));
    exit(1);
  }

  // Listen
  if (listen(socket_fd, MAX_CONNECTIONS) != 0) {
    char msg[] =
        "Something went wrong while setting up the socket for listening!";
    write(STDERR_FILENO, msg, sizeof(msg));
    exit(1);
  }

  while (1) {
    int* client_fd = (int*)malloc(sizeof(int));

    socklen_t client_len = sizeof(addr);

    // Accept
    if ((*client_fd = accept(socket_fd, (struct sockaddr*)&addr, &client_len)) <
        0) {
      char msg[] = "Could not accept the request!";
      write(STDERR_FILENO, msg, sizeof(msg));
      exit(1);
    }

    // Thread
    pthread_t thread;
    if (pthread_create(&thread, NULL, &thread_fn, (void*)client_fd) != 0) {
      char msg[] = "Could not create a thread!";
      write(STDERR_FILENO, msg, sizeof(msg));
      free(client_fd);
      exit(1);
    }
  }
  return 0;
}