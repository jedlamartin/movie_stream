#define _POSIX_C_SOURCE 200809L
#include "site.h"

#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char error_response[] =
    "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n";

// --- Prototypes ---
void urldecode(char* dst, const char* src);
void urlencode(char* dest, const char* src);
void makeabsolute(char* dest, const char* src);
void getcontenttype(char* dest, const char* filename);
int getcontentrange(char* content, off_t* start, off_t* end);
void normalizeranges(off_t* start, off_t* end, const off_t file_size);
int exists(const char* path);

void* thread_fn(void* arg) {
    int client_fd = *((int*) arg);
    free(arg);
    Header header = {
        .keep_alive = true, .range_request = false, .headers = NULL};

    while(header.keep_alive) {
        char buffer[BUFFER_SIZE];
        ssize_t read_bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if(read_bytes <= 0) {
            close(client_fd);
            pthread_exit((void*) 1);
        }
        buffer[read_bytes] = '\0';
        // printf("Request:\n%s\n", buffer); // <-- REMOVED LOGGING

        if(!strstr(buffer, "\r\n\r\n")) {
            close(client_fd);
            pthread_exit(NULL);
        }

        char* save_ptr;
        char* token = strtok_r(buffer, " \r\n", &save_ptr);
        if(!token) {
            close(client_fd);
            pthread_exit(NULL);
        }
        strncpy(header.method, token, sizeof(header.method) - 1);

        token = strtok_r(NULL, " \r\n", &save_ptr);
        if(!token) {
            close(client_fd);
            pthread_exit(NULL);
        }

        char* query_start = strchr(token, '?');
        if(query_start) {
            // Split path and query
            *query_start = '\0';
            query_start++;    // Move past '?'
            strncpy(header.query, query_start, sizeof(header.query) - 1);
            header.query[sizeof(header.query) - 1] = '\0';
        } else {
            header.query[0] = '\0';
        }

        char tmp[BUFFER_SIZE];
        size_t path_len = strlen(token);
        if(path_len >= sizeof(header.path)) path_len = sizeof(header.path) - 1;
        strncpy(header.path, token, path_len);
        header.path[path_len] = '\0';

        urldecode(tmp, header.path);
        makeabsolute(header.path, tmp);

        token = strtok_r(NULL, " \r\n", &save_ptr);
        strcpy(header.version, token ? token : "");

        char* content_type = strtok_r(NULL, ":", &save_ptr);
        char* content = strtok_r(NULL, "\r\n", &save_ptr);
        header.headers = create_list(content_type, content);
        while(content && content_type) {
            append_list(header.headers, content_type, content);
            if(strcasecmp(content_type, "Range") == 0) {
                if(getcontentrange(
                       content, &header.range_start, &header.range_end) == 0)
                    header.range_request = true;
            }
            content_type = strtok_r(NULL, ": \r\n", &save_ptr);
            content = strtok_r(NULL, "\r\n", &save_ptr);
        }

        if(header.path[0] != '/') {
            close(client_fd);
            pthread_exit(NULL);
        }
        memmove(header.path, header.path + 1, strlen(header.path));
        if(strcmp(header.path, "") == 0) strcpy(header.path, ".");

        int file_fd = -1;
        if((file_fd = open(header.path, O_RDONLY)) < 0) {
            write(client_fd,
                  "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n",
                  45);
            header.keep_alive = false;
        } else {
            struct stat st;
            fstat(file_fd, &st);

            if(S_ISDIR(st.st_mode)) {
                char index_path[PATH_MAX + 16];
                snprintf(index_path,
                         sizeof(index_path),
                         "%s/index.html",
                         header.path);

                if(file_exists(index_path)) {
                    if(header.path[strlen(header.path) - 1] != '/') {
                        char redirect_resp[BUFFER_SIZE];
                        snprintf(redirect_resp,
                                 sizeof(redirect_resp),
                                 "HTTP/1.1 302 Found\r\n"
                                 "Location: /%s/\r\n"
                                 "Connection: close\r\n\r\n",
                                 header.path);
                        write(client_fd, redirect_resp, strlen(redirect_resp));
                        close(file_fd);
                        free_list(header.headers);
                        close(client_fd);
                        pthread_exit(NULL);
                    } else {
                        close(file_fd);
                        strcpy(header.path, index_path);
                        file_fd = open(header.path, O_RDONLY);
                        fstat(file_fd, &st);
                    }
                }
            }

            if(S_ISREG(st.st_mode)) {
                char resp[BUFFER_SIZE];
                resp[0] = '\0';
                char content_type_str[256];
                getcontenttype(content_type_str, header.path);

                if(header.range_request) {
                    strcat(resp,
                           "HTTP/1.1 206 Partial Content\r\nContent-Range: "
                           "bytes ");
                    char r_start[32], r_end[32], r_total[32];
                    normalizeranges(
                        &header.range_start, &header.range_end, st.st_size);
                    sprintf(r_start, "%jd", (intmax_t) header.range_start);
                    sprintf(r_end, "%jd", (intmax_t) header.range_end);
                    sprintf(r_total, "%jd", (intmax_t) st.st_size);
                    strcat(resp, r_start);
                    strcat(resp, "-");
                    strcat(resp, r_end);
                    strcat(resp, "/");
                    strcat(resp, r_total);
                    strcat(resp, "\r\n");
                    strcat(resp, "Connection: close\r\n");
                    char cl[64];
                    sprintf(
                        cl,
                        "Content-Length: %jd\r\n",
                        (intmax_t) (header.range_end - header.range_start + 1));
                    strcat(resp, cl);
                    strcat(resp, "Content-Type: ");
                    strcat(resp, content_type_str);
                    strcat(resp, "\r\n\r\n");
                    write(client_fd, resp, strlen(resp));
                    lseek(file_fd, header.range_start, SEEK_SET);
                    off_t remaining =
                        (header.range_end - header.range_start + 1);
                    while(
                        remaining > 0 &&
                        (read_bytes = read(file_fd,
                                           buffer,
                                           (remaining < (off_t) sizeof(buffer) ?
                                                remaining :
                                                (off_t) sizeof(buffer)))) > 0) {
                        write(client_fd, buffer, read_bytes);
                        remaining -= read_bytes;
                    }
                } else {
                    strcat(resp,
                           "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\n");
                    char cl[64];
                    sprintf(
                        cl, "Content-Length: %jd\r\n", (intmax_t) st.st_size);
                    strcat(resp, cl);
                    strcat(resp, "Content-Type: ");
                    strcat(resp, content_type_str);
                    strcat(resp, "\r\n\r\n");
                    write(client_fd, resp, strlen(resp));
                    while((read_bytes = read(file_fd, buffer, sizeof(buffer))) >
                          0)
                        write(client_fd, buffer, read_bytes);
                }
            } else if(S_ISDIR(st.st_mode)) {
                // Directory listing logic
                const char resp_prefix[] =
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: "
                    "keep-alive\r\nContent-Length: ";
                const char resp_suffix[] = "\r\n\r\n";
                const char html_prefix[] =
                    "<h1>Directory Listing</h1>Directory: ";

                // --- RESTORED TO ORIGINAL LIST FORMAT ---
                const char list_begin[] = "<hr><ul>";
                const char list_end[] = "</ul><hr>";

                char body[BODY_MAX_SIZE];
                body[0] = '\0';
                strcat(body, html_prefix);
                strcat(body, header.path);
                strcat(body, list_begin);

                DIR* dir = opendir(header.path);
                struct dirent* dirent;
                while((dirent = readdir(dir)) != NULL) {
                    if(strcmp(dirent->d_name, ".") != 0 &&
                       strcmp(dirent->d_name, "..") != 0) {
                        char path[PATH_MAX], encoded[PATH_MAX * 3];
                        if(strcmp(header.path, ".") != 0) {
                            strcpy(path, header.path);
                            strcat(path, "/");
                            strcat(path, dirent->d_name);
                        } else {
                            strcpy(path, dirent->d_name);
                        }
                        urlencode(encoded, path);

                        // 1. Open List Item
                        strcat(body, "<li>");

                        // 2. The File Link
                        strcat(body, "<a href=\"/");
                        strcat(body, encoded);
                        strcat(body, "\">");
                        strcat(body, dirent->d_name);
                        strcat(body, "</a>");

                        // 3. Close List Item
                        strcat(body, "</li>");
                    }
                }
                closedir(dir);
                strcat(body, list_end);

                char cl[32];
                sprintf(cl, "%ld", strlen(body));
                write(client_fd, resp_prefix, strlen(resp_prefix));
                write(client_fd, cl, strlen(cl));
                write(client_fd, resp_suffix, strlen(resp_suffix));
                write(client_fd, body, strlen(body));
            }
            close(file_fd);
        }
        free_list(header.headers);
    }
    close(client_fd);
    pthread_exit(NULL);
}

// Helpers
void urldecode(char* dst, const char* src) {
    char a, b;
    while(*src) {
        if((*src == '%') && ((a = src[1]) && (b = src[2])) &&
           (isxdigit(a) && isxdigit(b))) {
            if(a >= 'a') a -= 'a' - 'A';
            if(a >= 'A') a -= ('A' - 10);
            else
                a -= '0';
            if(b >= 'a') b -= 'a' - 'A';
            if(b >= 'A') b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

void urlencode(char* dest, const char* src) {
    const char* hex = "0123456789abcdef";
    int pos = 0;
    for(size_t i = 0; i < strlen(src); i++) {
        unsigned char c = src[i];
        if(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
           ('0' <= c && c <= '9') || c == '/' || c == '-' || c == '_' ||
           c == '.' || c == '~') {
            dest[pos++] = c;
        } else {
            dest[pos++] = '%';
            dest[pos++] = hex[c >> 4];
            dest[pos++] = hex[c & 15];
        }
    }
    dest[pos] = '\0';
}

void makeabsolute(char* dest, const char* src) {
    const char* start = src;
    const char* end = strchr(start, '/');
    size_t len = end ? (size_t) (end - start) : strlen(start);
    strncpy(dest, start, len);
    dest[len] = '\0';
    while(end != NULL) {
        start = end + 1;
        end = strchr(start, '/');
        len = end ? (size_t) (end - start) : strlen(start);
        char token[BUFFER_SIZE];
        strncpy(token, start, len);
        token[len] = '\0';
        if(strcmp(token, ".") == 0 || strcmp(token, "..") == 0) {
            continue;
        } else {
            strcat(dest, "/");
            strcat(dest, token);
        }
    }
}

void getcontenttype(char* dest, const char* filename) {
    const char* index = strrchr(filename, '.');
    if(!index) {
        strcpy(dest, "application/octet-stream");
        return;
    }

    // Core Media
    if(strcmp(index, ".mkv") == 0)
        strcpy(dest, "video/mp4");    // Legacy raw mapping
    else if(strcmp(index, ".mp4") == 0)
        strcpy(dest, "video/mp4");

    // HLS Streaming Essentials (CRITICAL for Safari/iOS support)
    else if(strcmp(index, ".m3u8") == 0)
        strcpy(dest, "application/vnd.apple.mpegurl");
    else if(strcmp(index, ".ts") == 0)
        strcpy(dest, "video/mp2t");
    else if(strcmp(index, ".vtt") == 0)
        strcpy(dest, "text/vtt");

    // Standard Web Types
    else if(strcmp(index, ".html") == 0)
        strcpy(dest, "text/html; charset=utf-8");
    else if(strcmp(index, ".css") == 0)
        strcpy(dest, "text/css");
    else if(strcmp(index, ".js") == 0)
        strcpy(dest, "application/javascript");
    else if(strcmp(index, ".png") == 0)
        strcpy(dest, "image/png");
    else if(strcmp(index, ".jpg") == 0 || strcmp(index, ".jpeg") == 0)
        strcpy(dest, "image/jpeg");

    else
        strcpy(dest, "application/octet-stream");
}

int getcontentrange(char* content, off_t* start, off_t* end) {
    char* p = strstr(content, "bytes=");
    if(!p) return -1;
    p += 6;
    char buf[128];
    strncpy(buf, p, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* dash = strchr(buf, '-');
    if(!dash) return -1;
    *dash = '\0';
    *start = atoll(buf);
    if(*(dash + 1)) *end = atoll(dash + 1);
    else
        *end = -1;
    return 0;
}

void normalizeranges(off_t* start, off_t* end, const off_t file_size) {
    if(*end == -1) *end = file_size - 1;
    if(*start < 0) *start = 0;
    if(*end >= file_size) *end = file_size - 1;
}

int exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}
