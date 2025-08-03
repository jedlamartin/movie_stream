#include "site.h"

void* thread_fn(void* arg) {
  int client_fd = *((int*)arg);
  free(arg);

  char buffer[BUFFER_SIZE];
  memset(buffer, 0, BUFFER_SIZE);

  ssize_t read_bytes = read(client_fd, buffer, BUFFER_SIZE - 1);
  if (read_bytes > 0) {
    buffer[read_bytes] = '\0';
    printf("Request:\n%s\n", buffer);  // debug log
  }

  // Reading index.html
  int index_fd = -1;
  if ((index_fd = open("index.html", O_RDONLY)) < 0) {
    char msg[] = "Could not open index.html!";
    write(STDERR_FILENO, msg, sizeof(msg));
    const char* error_response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "404 Not Found - Could not open index.html";
    write(client_fd, error_response, strlen(error_response));
    close(client_fd);
    pthread_exit((void*)1);
  }

  // Constructing and sending the header
  off_t offset = (off_t)-1;
  if ((offset = lseek(index_fd, 0, SEEK_END)) < (off_t)0) {
    char msg[] = "Could not construct header!";
    write(STDERR_FILENO, msg, sizeof(msg));
    pthread_exit((void*)1);
  }
  lseek(index_fd, 0, SEEK_SET);

  char header[BUFFER_SIZE] =
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ";
  if ((snprintf(header + strlen(header), BUFFER_SIZE - strlen(header) - 1,
                "%lu%s", offset, "\r\n\r\n")) < 0) {
    char msg[] = "Could not construct header!";
    write(STDERR_FILENO, msg, sizeof(msg));
    pthread_exit((void*)1);
  }
  printf("%s\n", header);
  if (write(client_fd, header, strlen(header)) < 0) {
    char msg[] = "Could not send header!";
    write(STDERR_FILENO, msg, sizeof(msg));
    pthread_exit((void*)1);
  }

  while (read(index_fd, buffer, BUFFER_SIZE) > 0) {
    write(client_fd, buffer, strlen(buffer));
    printf("%s", buffer);
  }
  close(index_fd);

  // Polling for password
  if (read(client_fd, buffer, BUFFER_SIZE - 1) < 0) {
    char msg[] = "Could not read answer!";
    write(STDERR_FILENO, msg, sizeof(msg));
    pthread_exit((void*)1);
  }
  if (strstr(buffer, "GET /?password=") == NULL) {
    const char* response =
        "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n\r\n";
    write(client_fd, response, strlen(response));
    close(client_fd);
    pthread_exit(NULL);
  }

  printf("Request with password:\n%s\n", buffer);  // debug log

  // Processing the request with the password
  char* token = strtok(buffer, " ");
  while (token != NULL && strstr(token, "/?password=") == NULL) {
    token = strtok(NULL, " ");
  }
  char* password = strchr(token, '=') + 1;
  printf("\n%s\n", password);

  // Check password
  if (strcmp("azorvosfia", password) != 0) {
    const char* error_response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "404 Not Found";
    write(client_fd, error_response, strlen(error_response));
    close(client_fd);
    pthread_exit(NULL);
  }

  // Open film website
  int movies_fd = -1;
  if ((movies_fd = open("movies.html", O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
    char msg[] = "Could not open movies.html!";
    write(STDERR_FILENO, msg, sizeof(msg));
    pthread_exit((void*)1);
  }

  export(movies_fd);

  lseek(movies_fd, 0, SEEK_SET);
  offset = lseek(movies_fd, 0, SEEK_END);
  lseek(movies_fd, 0, SEEK_SET);

  strcpy(header,
         "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: ");
  if ((snprintf(header + strlen(header), BUFFER_SIZE - strlen(header) - 1,
                "%lu%s", offset, "\r\n\r\n")) < 0) {
    char msg[] = "Could not construct header!";
    write(STDERR_FILENO, msg, sizeof(msg));
    pthread_exit((void*)1);
  }

  while (read(movies_fd, buffer, BUFFER_SIZE) > 0) {
    write(movies_fd, buffer, strlen(buffer));
    printf("%s", buffer);
  }
  close(movies_fd);

  close(client_fd);
  pthread_exit(NULL);
}