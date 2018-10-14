#define _GNU_SOURCE
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// List to keep track of socket file descriptors
#define MAX_CONN_NUMBER 10000

static int sfds[MAX_CONN_NUMBER];

static void handle_error(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static void *handle_session(void *data)
{
	return NULL;
}

int main(int argc, char *argv[])
{
	int sfd, conn_numbr;
	socklen_t client_addr_len;
	struct sockaddr_in server_addr, client_addr;
	if (argc < 2) {
		char buff[50];
		sprintf(buff, "Usage: %s [port]\n", argv[0]);
		handle_error(buff);
	}

	int port = (int) strtol(argv[1], NULL, 10);
	// Create socket fd
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd < 0){
		handle_error("Unable to create socket descriptor\n");
	}
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);
	if (bind(sfd, (struct sockaddr *) &server_addr,
				sizeof(struct sockaddr_in)) == -1) {
		handle_error("Error binding to address\n");
	}
	if (listen(sfd, 5) == -1) {
		handle_error("Error preparing socket for listening\n");
	}
	conn_numbr = 0;
	client_addr_len = sizeof(client_addr);
	printf("Socket server initialized at port %d\n", port);
	while (1) {
		sfds[conn_numbr] = accept4(sfd,
				(struct sockaddr *) &client_addr,
				&client_addr_len, SOCK_CLOEXEC);
		if (sfds[conn_numbr] < 0) {
			handle_error("Error accepting connection\n");
		}
		printf("Incoming connection from:\n");
		printf("Address: %d\n", client_addr.sin_addr.s_addr);
		printf("Port: %d\n", client_addr.sin_port);
		pthread_t thread;
		if (pthread_create(&thread,
					NULL, handle_session,
					(void *) &sfds[conn_numbr]) != 0) {
			handle_error("Unable to create session thread\n");
		}
		printf("Pthread Created\n");
		pthread_join(thread, NULL);
	}







	return 0;
}
