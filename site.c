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
int getcontentrange(char* content, size_t* start, size_t* end);

void* thread_fn(void* arg) {
	int client_fd = *((int*)arg);
	free(arg);

	Header header = {
	.version = "",
	.method = "",
	.path = "",
	.keep_alive = true,
	.range_request = false,
	.range_start = 0,
	.range_end = 0,
	.headers = NULL,
	};

	while (header.keep_alive) {
		char buffer[BUFFER_SIZE];
		ssize_t read_bytes = read(client_fd, buffer, BUFFER_SIZE - 1);
		if (read_bytes <= 0) {
			fprintf(stderr, "Could not read from socket!");
			close(client_fd);
			pthread_exit((void*)1);
		}
		buffer[read_bytes] = '\0';
		printf("Request:\n%s\n", buffer);  // debug log


		// Parsing the header
		header.range_request = false;

		if (!strstr(buffer, "\r\n\r\n")) {
			fprintf(stderr, "Header too large or malformed!");
			write(client_fd, error_response, strlen(error_response));
			close(client_fd);
			pthread_exit((void*)1);
		}

		char* save_ptr;
		char* token = strtok_r(buffer, " \r\n", &save_ptr);
		if (!token) {
			fprintf(stderr, "Invalid request format!");
			write(client_fd, error_response, strlen(error_response));
			close(client_fd);
			pthread_exit((void*)1);
		}
		strncpy(header.method, token, sizeof(header.method) - 1);
		header.method[sizeof(header.method) - 1] = '\0'; // Ensure null termination
		token = strtok_r(NULL, " \r\n", &save_ptr);
		if (!token) {
			fprintf(stderr, "Invalid request format!");
			write(client_fd, error_response, strlen(error_response));
			close(client_fd);
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
			write(client_fd, error_response, strlen(error_response));
			close(client_fd);
			pthread_exit((void*)1);
		}
		strcpy(header.version, token);

		char* content_type = strtok_r(NULL, ": \r\n", &save_ptr);
		char* content = strtok_r(NULL, "\r\n", &save_ptr);
		header.headers = create_list(content_type, content);

		//printf("Method:%s\n", header.method);
		//printf("Path:%s\n", header.path);
		//printf("Version:%s\n", header.version);

		if (strcmp(header.version, "HTTP/1.1") == 0) {

			while (content != NULL && content_type != NULL) {
				append_list(header.headers, content_type, content);

				if (strcasecmp(content_type, "Connection") == 0 && strcasecmp(content, "closed") == 0) {
					header.keep_alive = false;
				}
				else if (strcasecmp(content_type, "Range") == 0) {
					if (getcontentrange(content, &header.range_start, &header.range_end) == 0) {
						header.range_request = true;
					}
					else {
						fprintf(stderr, "Invalid range!");
						write(client_fd, error_response, strlen(error_response));
						free_list(header.headers);
						close(client_fd);
						pthread_exit((void*)1);
					}
				}
				content_type = strtok_r(NULL, ": \r\n", &save_ptr);
				content = strtok_r(NULL, "\r\n", &save_ptr);

			}
		}
		else if (strcmp(header.version, "HTTP/1.0") == 0) {
			// HTTP/1.0 does not support keep-alive by default
			header.keep_alive = false;
		}
		else
		{
			fprintf(stderr, "Unsupported HTTP version!");
			free_list(header.headers);
			write(client_fd, error_response, strlen(error_response));
			close(client_fd);
			pthread_exit((void*)1);
		}

		if (strcmp(header.method, "GET") != 0) {
			fprintf(stderr, "Unsupported HTTP method!");
			free_list(header.headers);
			write(client_fd, error_response, strlen(error_response));
			close(client_fd);
			pthread_exit((void*)1);
		}
		if (header.path[0] != '/') {
			fprintf(stderr, "The path must be absolute!");
			free_list(header.headers);
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
			header.keep_alive = false;
			const char resp[] =
				"HTTP/1.1 404 Not Found\r\n"
				"Connection: close\r\n"
				"\r\n";
			write(client_fd, resp, sizeof(resp) - 1);
		}
		else {
			struct stat st;
			fstat(file_fd, &st);
			if (S_ISREG(st.st_mode)) {
				char resp[BUFFER_SIZE] = "";
				size_t content_length;

				if (header.range_request) {
					strcat(resp, "HTTP/1.1 206 Partial Content\r\n");

					strcat(resp, "Content-Range: bytes ");
					char content_start[20];
					sprintf(content_start, "%ld", header.range_start);
					strcat(resp, content_start);
					strcat(resp, "-");
					char content_end[20];
					header.range_end ? sprintf(content_end, "%ld", header.range_end) : sprintf(content_end, "%ld", st.st_size - 1);
					strcat(resp, content_end);
					strcat(resp, "/");
					char content_size[20];
					sprintf(content_size, "%ld", st.st_size);
					strcat(resp, content_size);
					strcat(resp, "\r\n");

					content_length = header.range_end ? header.range_end - header.range_start + 1 : st.st_size - header.range_start;

					strcat(resp, "Connection: ");

					lseek(file_fd, header.range_start, SEEK_SET);
				}
				else {
					strcat(resp, "HTTP/1.1 200 OK\r\n"
						"Connection: ");

					content_length = st.st_size;
				}
				printf("start: %ld, end: %ld, content length: %ld", header.range_start, header.range_end, content_length);

				header.keep_alive ? strcat(resp, "keep-alive\r\n") : strcat(resp, "closed\r\n");

				strcat(resp, "Content-Length: ");
				char content_length_str[20];
				sprintf(content_length_str, "%ld", content_length);
				strcat(resp, content_length_str);
				strcat(resp, "\r\n");


				strcat(resp, "Content-type: ");
				char content_type[256];
				getcontenttype(content_type, header.path);
				strcat(resp, content_type);
				strcat(resp, "\r\n\r\n");


				write(client_fd, resp, strlen(resp));

				off_t bytes_remaining = content_length;
				size_t to_read = bytes_remaining < BUFFER_SIZE ? bytes_remaining : BUFFER_SIZE;
				while ((read_bytes = read(file_fd, buffer, to_read)) > 0 && bytes_remaining > 0) {
					if (write(client_fd, buffer, read_bytes) != read_bytes) {
						fprintf(stderr, "Failed to send file completely!\n");
						break;
					}
					bytes_remaining -= read_bytes;
					to_read = bytes_remaining < BUFFER_SIZE ? bytes_remaining : BUFFER_SIZE;
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
		free_list(header.headers);
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

	char* index;

	if (!(index = strrchr(filename, '.'))) {
		strcpy(dest, "application/octet-stream");
		return;
	}

	if (strcmp(index, ".html") == 0 ||
		strcmp(index, ".htm") == 0) {
		strcpy(dest, "text/html");
	}
	else if (strcmp(index, ".css") == 0) {
		strcpy(dest, "text/css");
	}
	else if (strcmp(index, ".js") == 0) {
		strcpy(dest, "application/javascript");
	}
	else if (strcmp(index, ".json") == 0) {
		strcpy(dest, "application/json");
	}
	else if (strcmp(index, ".txt") == 0) {
		strcpy(dest, "text/plain");
	}
	else if (strcmp(index, ".png") == 0) {
		strcpy(dest, "image/png");
	}
	else if (strcmp(index, ".jpg") == 0 ||
		strcmp(index, ".jpeg") == 0) {
		strcpy(dest, "image/jpeg");
	}
	else if (strcmp(index, ".gif") == 0) {
		strcpy(dest, "image/gif");
	}
	else if (strcmp(index, ".svg") == 0) {
		strcpy(dest, "image/svg+xml");
	}
	else if (strcmp(index, ".ico") == 0) {
		strcpy(dest, "image/x-icon");
	}
	else if (strcmp(index, ".pdf") == 0) {
		strcpy(dest, "application/pdf");
	}
	else if (strcmp(index, ".zip") == 0) {
		strcpy(dest, "application/zip");
	}
	else {
		strcpy(dest, "application/octet-stream");
	}
}

int getcontentrange(char* content, size_t* start, size_t* end) {
	// Example: bytes=500-999
	char* start_str = NULL;
	if (!(start_str = strstr(content, "bytes="))) {
		return -1;
	}
	start_str += strlen("bytes=");

	char* dash_ptr = NULL;
	long start_val = strtol(start_str, &dash_ptr, 10);
	if ( *start_str != '-' && (*dash_ptr != '-' || start_val < 0)) {
		return -1;
	}

	char* end_str = strchr(content, '-')+1;
	char* end_ptr = NULL;
	long end_val = strtol(end_str, &end_ptr, 10);

	if (end_val < 0 || (end_val>0 && end_val < start_val)) {
		return -1;
	}

	if (*start_str == '-') {
		// No start specified
		*start = 0;
	}
	else {
		*start = (size_t)start_val;
	}

	if (end_str == end_ptr) {
		// No end specified
		*end = 0;
	}
	else {
		*end = (size_t)end_val;
	}
	return 0;
}