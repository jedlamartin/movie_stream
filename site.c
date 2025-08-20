#include "site.h"

const char error_response[] =
"HTTP/1.1 400 Bad Request\r\n"
"Content-Type: text/plain\r\n"
"Connection: close\r\n"
"\r\n";


void urldecode(char* dst, const char* src);
void urlencode(char* dest, const char* src);
void makeabsolute(char* dest, const char* src);
void getcontenttype(char* dest, const char* filename);

void* thread_fn(void* arg) {
	int client_fd = *((int*)arg);
	free(arg);

	bool keep_alive = true;

	while (keep_alive) {
		char buffer[BUFFER_SIZE];
		ssize_t read_bytes = read(client_fd, buffer, BUFFER_SIZE - 1);
		if (read_bytes <= 0) {
			close(client_fd);
			pthread_exit((void*)1);
		}
		buffer[read_bytes] = '\0';
		printf("Request:\n%s\n", buffer);  // debug log


		// Parsing the header
		Header header;

		if (!strstr(buffer, "\r\n\r\n")) {
			// header too large or malformed
			write(client_fd, error_response, sizeof(error_response) - 1);
			close(client_fd);
			pthread_exit((void*)1);
		}

		char* save_ptr;
		char* token = strtok_r(buffer, " \r\n", &save_ptr);
		if (!token) {
			fprintf(stderr, "Invalid request format!");
			pthread_exit((void*)1);
		}
		strncpy(header.method, token, sizeof(header.method) - 1);
		header.method[sizeof(header.method) - 1] = '\0'; // Ensure null termination
		token = strtok_r(NULL, " \r\n", &save_ptr);
		if (!token) {
			fprintf(stderr, "Invalid request format!");
			pthread_exit((void*)1);
		}
		// Decode the url
		char* query = strchr(token, '?');
		char tmp[BUFFER_SIZE];
		query ? strncpy(header.path, token, query - token)
			: strncpy(header.path, token, sizeof(header.path) - 1);
		header.path[sizeof(header.path) - 1] = '\0'; // Ensure null termination
		urldecode(tmp, header.path);
		makeabsolute(header.path, tmp);
		token = strtok_r(NULL, " \r\n", &save_ptr);
		if (!token) {
			fprintf(stderr, "Invalid request format!");
			pthread_exit((void*)1);
		}
		strcpy(header.version, token);

		char* content_type = strtok_r(NULL, ": \r\n", &save_ptr);
		char* content = strtok_r(NULL, ": \r\n", &save_ptr);
		List* list = create_list(content_type, content);

		//printf("Method:%s\n", header.method);
	//printf("Path:%s\n", header.path);
	//printf("Version:%s\n", header.version);

		if (strcmp(header.version, "HTTP/1.1") == 0) {

			while (content != NULL && content_type != NULL) {
				if (strcmp(content_type, "Connection") == 0 && strcmp(content, "closed") == 0) {
					keep_alive = false;
				}
				content_type = strtok_r(NULL, ": \r\n", &save_ptr);
				content = strtok_r(NULL, ": \r\n", &save_ptr);
				if (content_type != NULL && content != NULL) {
					append_list(list, content_type, content);
				}
			}
		}
		else if (strcmp(header.version, "HTTP/1.0") == 0) {
			// HTTP/1.0 does not support keep-alive by default
			keep_alive = false;
		}
		else
		{
			fprintf(stderr, "Unsupported HTTP version!");
			free_list(list);
			write(client_fd, error_response, strlen(error_response));
			close(client_fd);
			pthread_exit((void*)1);
		}

		if (strcmp(header.method, "GET") != 0) {
			fprintf(stderr, "Unsupported HTTP method!");
			free_list(list);
			write(client_fd, error_response, strlen(error_response));
			close(client_fd);
			pthread_exit((void*)1);
		}
		if (header.path[0] != '/') {
			fprintf(stderr, "The path must be absolute!");
			free_list(list);
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
		}
		else {
			struct stat st;
			fstat(file_fd, &st);
			if (S_ISREG(st.st_mode)) {
				char resp[BUFFER_SIZE] =
					"HTTP/1.1 200 OK\r\n"
					"Connection: ";

				keep_alive ? strcat(resp, "keep-alive\r\n") : strcat(resp, "closed\r\n");

				strcat(resp, "Content-type: ");
				char content_type[256];
				getcontenttype(content_type, header.path);

				strcat(resp, content_type);
				strcat(resp, "\r\n");

				strcat(resp, "Content-Length: ");
				char content_length[20];
				sprintf(content_length, "%ld\r\n\r\n", st.st_size);
				strcat(resp, content_length);

				write(client_fd, resp, strlen(resp));
				while ((read_bytes = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
					write(client_fd, buffer, read_bytes);
				}
			}
			else if (S_ISDIR(st.st_mode)) {
				const char resp_prefix[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nContent-Length: ";
				const char resp_suffix[] = "\r\n\r\n";

				const char html_prefix[] = "<h1>Directory Listing</h1>Directory: ";
				const char list_begin[] = "<hr><ul>";
				const char list_end[] = "</ul><hr>";
				const char entry_begin[] = "<li><a href=\"/";
				const char entry_mid[] = "\">";
				const char entry_end[] = "</a></li>";

				char body[BODY_MAX_SIZE]; body[0] = '\0';
				strcat(body, html_prefix);
				strcat(body, header.path);
				strcat(body, list_begin);

				DIR* dir = opendir(header.path);
				struct dirent* dirent = NULL;
				char path[PATH_MAX];
				while ((dirent = readdir(dir)) != NULL) {
					if (strcmp(dirent->d_name, ".") != 0 &&
						strcmp(dirent->d_name, "..") != 0) {

						if (strcmp(header.path, ".") != 0) {
							strcpy(path, header.path);
							strcat(path, "/");
							strcat(path, dirent->d_name);
						}
						else {
							strcpy(path, dirent->d_name);
						}

						char encoded[PATH_MAX * 3];
						urlencode(encoded, path);
						strcat(body, entry_begin);
						strcat(body, encoded);
						strcat(body, entry_mid);
						strcat(body, dirent->d_name);
						strcat(body, entry_end);
					}
				};
				closedir(dir);
				strcat(body, list_end);

				// Send everything
				char content_length[20];
				sprintf(content_length, "%ld", strlen(body));

				// Header
				write(client_fd, resp_prefix, strlen(resp_prefix));
				write(client_fd, content_length, strlen(content_length));
				write(client_fd, resp_suffix, strlen(resp_suffix));

				// Body
				write(client_fd, body, strlen(body));

			}

			close(file_fd);
		}
		// Free the linked list
		free_list(list);
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
		}
		else if (*src == '+') {
			*dst++ = ' ';
			src++;
		}
		else {
			*dst++ = *src++;
		}
	}
	*dst++ = '\0';
}

void urlencode(char* dest, const char* src) {
	const char* hex = "0123456789abcdef";
	int pos = 0;

	for (int i = 0; i < strlen(src); i++) {
		unsigned char c = src[i];
		if (('a' <= c && c <= 'z') ||
			('A' <= c && c <= 'Z') ||
			('0' <= c && c <= '9') ||
			c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
			// Leave safe characters as-is
			dest[pos++] = c;
		}
		else {
			dest[pos++] = '%';
			dest[pos++] = hex[c >> 4];
			dest[pos++] = hex[c & 15];
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
		}
		else {
			// Append new segment
			strcat(dest, "/");
			strcat(dest, token);
		}
	}
}

void getcontenttype(char* dest, const char* filename) {


	if (!strrchr(filename, '.')) {
		strcpy(dest, "application/octet-stream");
		return;
	}

	if (strcmp(strrchr(filename, '.'), ".html") == 0 ||
		strcmp(strrchr(filename, '.'), ".htm") == 0) {
		strcpy(dest, "text/html");
	}
	else if (strcmp(strrchr(filename, '.'), ".css") == 0) {
		strcpy(dest, "text/css");
	}
	else if (strcmp(strrchr(filename, '.'), ".js") == 0) {
		strcpy(dest, "application/javascript");
	}
	else if (strcmp(strrchr(filename, '.'), ".json") == 0) {
		strcpy(dest, "application/json");
	}
	else if (strcmp(strrchr(filename, '.'), ".txt") == 0) {
		strcpy(dest, "text/plain");
	}
	else if (strcmp(strrchr(filename, '.'), ".png") == 0) {
		strcpy(dest, "image/png");
	}
	else if (strcmp(strrchr(filename, '.'), ".jpg") == 0 ||
		strcmp(strrchr(filename, '.'), ".jpeg") == 0) {
		strcpy(dest, "image/jpeg");
	}
	else if (strcmp(strrchr(filename, '.'), ".gif") == 0) {
		strcpy(dest, "image/gif");
	}
	else if (strcmp(strrchr(filename, '.'), ".svg") == 0) {
		strcpy(dest, "image/svg+xml");
	}
	else if (strcmp(strrchr(filename, '.'), ".ico") == 0) {
		strcpy(dest, "image/x-icon");
	}
	else if (strcmp(strrchr(filename, '.'), ".pdf") == 0) {
		strcpy(dest, "application/pdf");
	}
	else if (strcmp(strrchr(filename, '.'), ".zip") == 0) {
		strcpy(dest, "application/zip");
	}
	else {
		strcpy(dest, "application/octet-stream");
	}
}