#include "site.h"

const char error_response[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n";

void urldecode(char* dst, const char* src);
void urlencode(char* dest, const char* src);
void makeabsolute(char* dest, const char* src);
void getcontenttype(char* dest, const char* filename);

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

  // Parsing the header
  Header header;
  char* token = strtok(buffer, " \r\n");
  strcpy(header.method, token);
  token = strtok(NULL, " \r\n");
  // Decode the url
  char* query = strchr(token, '?');
  char tmp[BUFFER_SIZE];
  query ? strncpy(header.path, token, query - token)
        : strcpy(header.path, token);
  urldecode(tmp, header.path);
  makeabsolute(header.path, tmp);
  token = strtok(NULL, " \r\n");
  strcpy(header.version, token);

  printf("Method:%s\n", header.method);
  printf("Path:%s\n", header.path);
  printf("Version:%s\n", header.version);

  if (strcmp(header.version, "HTTP/1.1") != 0) {
    fprintf(stderr, "Unsupported HTTP version!");
    write(client_fd, error_response, strlen(error_response));
    close(client_fd);
    pthread_exit((void*)1);
  }

  if (strcmp(header.method, "GET") != 0) {
    fprintf(stderr, "Unsupported HTTP method!");
    write(client_fd, error_response, strlen(error_response));
    close(client_fd);
    pthread_exit((void*)1);
  }
  if (header.path[0] != '/') {
    fprintf(stderr, "The path must be absolute!");
    write(client_fd, error_response, strlen(error_response));
    close(client_fd);
    pthread_exit((void*)1);
  }
  memmove(header.path, header.path + 1, strlen(header.path));

  if (strcmp(header.path, "") == 0) {
    strcpy(header.path, ".");
  }

  int file_fd = -1;
  if ((file_fd = open(header.path, O_RDONLY)) < 0) {
    const char resp[] =
        "HTTP/1.1 404 Not Found\r\n"
        "\r\n";
    write(client_fd, resp, strlen(resp));
  } else {
    struct stat stat;
    fstat(file_fd, &stat);
    if (S_ISREG(stat.st_mode)) {
      char resp[BUFFER_SIZE] =
          "HTTP/1.1 200 OK\r\n"
          "Content-type: text/plain\r\n"
          "\r\n";

      char content_type[256];
      getcontenttype(content_type, header.path);
      strcat(resp, content_type);
      strcat(resp, "\r\n\r\n");
      write(client_fd, resp, strlen(resp));
      while ((read_bytes = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        write(client_fd, buffer, read_bytes);
      }
    } else if (S_ISDIR(stat.st_mode)) {
      const char resp[] =
          "HTTP/1.1 200 OK\r\n"
          "Content-Type: text/html\r\n"
          "\r\n"
          "<h1>Directory Listing</h1>"
          "Directory: ";
      write(client_fd, resp, strlen(resp));
      write(client_fd, header.path, strlen(header.path));

      const char list_begin[] = "<hr><ul>";
      write(client_fd, list_begin, strlen(list_begin));

      const char entry_begin[] = "<li><a href=";
      const char entry_end[] = "</a></li>";
      DIR* dir = opendir(header.path);
      struct dirent* dirent = NULL;
      char path[PATH_MAX];
      char encoded[PATH_MAX * 3];
      while ((dirent = readdir(dir)) != NULL) {
        if (strcmp(dirent->d_name, ".") != 0 &&
            strcmp(dirent->d_name, "..") != 0) {
          write(client_fd, entry_begin, strlen(entry_begin));

          if (strcmp(header.path, ".") != 0) {
            strcpy(path, header.path);
            strcat(path, "/");
            strcat(path, dirent->d_name);
          } else {
            strcpy(path, dirent->d_name);
          }
          // printf("\n\nBefore: %s\n\n", path);

          urlencode(encoded, path);

          // printf("\n\nAfter: %s\n\n", encoded);

          write(client_fd, encoded, strlen(encoded));

          write(client_fd, ">", sizeof(char));
          write(client_fd, dirent->d_name, strlen(dirent->d_name));
          write(client_fd, entry_end, strlen(entry_end));
        }
      };
      closedir(dir);

      const char list_end[] = "</ul><hr>";
      write(client_fd, list_end, strlen(list_end));
    }

    close(file_fd);
  }

  close(client_fd);
  pthread_exit(NULL);
}

void urldecode(char* dst, const char* src) {
  char a, b;
  while (*src) {
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
        (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a') a -= 'a' - 'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a') b -= 'a' - 'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst++ = '\0';
}

void urlencode(char* dest, const char* src) {
  // dest should have a length of strlen(src) * 3 + 1

  const char* hex = "0123456789abcdef";

  int pos = 0;
  for (int i = 0; i < strlen(src); i++) {
    if (('a' <= src[i] && src[i] <= 'z') || ('A' <= src[i] && src[i] <= 'Z') ||
        ('0' <= src[i] && src[i] <= '9')) {
      dest[pos++] = src[i];
    } else {
      dest[pos++] = '%';
      dest[pos++] = hex[src[i] >> 4];
      dest[pos++] = hex[src[i] & 15];
    }
  }
  dest[pos] = '\0';
}

void makeabsolute(char* dest, const char* src) {
  // Initialize destination with first token
  const char* start = src;
  const char* end = strchr(start, '/');
  size_t len = end ? (size_t)(end - start) : strlen(start);

  strncpy(dest, start, len);
  dest[len] = '\0';

  // Process remaining tokens
  while (end != NULL) {
    start = end + 1;  // Skip '/'
    end = strchr(start, '/');
    len = end ? (size_t)(end - start) : strlen(start);

    char token[BUFFER_SIZE];
    strncpy(token, start, len);
    token[len] = '\0';

    // Skip "." and handle ".."
    if (strcmp(token, ".") == 0 || strcmp(token, "..") == 0) {
      continue;  // Skip current directory (.)
    } else {
      // Append new segment
      strcat(dest, "/");
      strcat(dest, token);
    }
  }
}

void getcontenttype(char* dest, const char* filename) {
  if (strcmp(strrchr(filename, '.'), ".html") == 0 ||
      strcmp(strrchr(filename, '.'), ".htm") == 0) {
    strcpy(dest, "tetx/html");
  } else if (strcmp(strrchr(filename, '.'), ".css") == 0) {
    strcpy(dest, "tetx/css");
  } else if (strcmp(strrchr(filename, '.'), ".js") == 0) {
    strcpy(dest, "application/javascript");
  } else if (strcmp(strrchr(filename, '.'), ".json") == 0) {
    strcpy(dest, "application/json");
  } else if (strcmp(strrchr(filename, '.'), ".txt") == 0) {
    strcpy(dest, "tetx/plain");
  } else if (strcmp(strrchr(filename, '.'), ".png") == 0) {
    strcpy(dest, "image/png");
  } else if (strcmp(strrchr(filename, '.'), ".jpg") == 0 ||
             strcmp(strrchr(filename, '.'), ".jpeg") == 0) {
    strcpy(dest, "image/jpeg");
  } else if (strcmp(strrchr(filename, '.'), ".gif") == 0) {
    strcpy(dest, "image/gif");
  } else if (strcmp(strrchr(filename, '.'), ".svg") == 0) {
    strcpy(dest, "image/svg+xml");
  } else if (strcmp(strrchr(filename, '.'), ".ico") == 0) {
    strcpy(dest, "image/x-icon");
  } else if (strcmp(strrchr(filename, '.'), ".pdf") == 0) {
    strcpy(dest, "application/pdf");
  } else if (strcmp(strrchr(filename, '.'), ".zip") == 0) {
    strcpy(dest, "application/zip");
  } else {
    strcpy(dest, "application/octet-stream");
  }
}
