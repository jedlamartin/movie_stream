#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "site.h"

#define PORT 8080                // Server listening port
#define MAX_CONNECTIONS 5        // Maximum simultaneous client connections

void printusage(char* progname, int fd);

int main(int argc, char* argv[]) {

	int opt = -1;
	int port = PORT;
	int max_connections = MAX_CONNECTIONS;

	while ((opt = getopt(argc, argv, "hp:c:")) != -1) {
		switch (opt) {
		case 'h':
			printusage(argv[0], STDOUT_FILENO);
			return 0;
			break;
		case 'p':
			if ((port = strtol(optarg, NULL, 10)) < 1) {
				printusage(argv[0], STDERR_FILENO);
				return 1;
			}
			break;
		case 'c':
			if ((max_connections = strtol(optarg, NULL, 10))< 1) {
				printusage(argv[0], STDERR_FILENO);
				return 1;
			}
			break;
		default:
			printusage(argv[0], STDERR_FILENO);
			return 1;
			break;
		}
	}


	int socket_fd = -1;
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		char msg[] = "Could not create socket!";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(1);
	}

	// Then set up SIGPIPE to be ignored
	struct sigaction sa = {
		.sa_handler = SIG_IGN,
		.sa_flags = 0,
	};
	if (sigemptyset(&sa.sa_mask) != 0 || sigaction(SIGPIPE, &sa, 0) != 0) {
		fprintf(stderr, "Could not ignore SIGPIPE: %s\n", strerror(errno));
		exit(1);
	}

	// Enabling port reuse
	opt = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
		fprintf(stderr, "setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
		exit(1);
	}

	// Bind
	struct sockaddr_in addr = { .sin_family = AF_INET,
          .sin_addr.s_addr = INADDR_ANY,
          .sin_port = htons(port) };
	if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
		char msg[] = "Could not bind socket!";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(1);
	}

	// Listen
	if (listen(socket_fd, max_connections) != 0) {
		fprintf(stderr, "listen() failed: %s\n", strerror(errno));
		exit(1);
	}

	while (1) {
		int* client_fd = (int*)malloc(sizeof(int));

		socklen_t client_len = sizeof(addr);

		// Accept
		if ((*client_fd = accept(socket_fd, (struct sockaddr*)&addr, &client_len)) <
			0) {
			fprintf(stderr, "accept() failed: %s\n", strerror(errno));
			exit(1);
		}

		// Thread
		pthread_t thread;
		if (pthread_create(&thread, NULL, &thread_fn, (void*)client_fd) != 0) {
			fprintf(stderr, "pthread_create() failed: %s\n", strerror(errno));
			free(client_fd);
			exit(1);
		}
	}
	return 0;
}

void printusage(char* progname, int fd){
	dprintf(fd, "Usage: %s [-h] [-p port] [-c max_connections]\n", progname);
	dprintf(fd, "  -h        Show this help message and exit\n");
	dprintf(fd, "  -p port   Specify the port to listen on (default: %d)\n", PORT);
	dprintf(fd, "  -c max_connections   Specify the maximum simultaneous client connections (default: %d)\n", MAX_CONNECTIONS);
}