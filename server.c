#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>

#define MESSAGE_BUFFER 500
#define CLIENT_ADDRESS_LENGTH 100

void * start_server (int socket_fd, struct sockaddr_in *address) {
	bind(socket_fd, (struct sockaddr *) address, sizeof *address);
	printf("Esperando conexion...\n");
	listen(socket_fd, 10);
}

void * send_message(int new_socket_fd, struct sockaddr *cl_addr) {
	char message[MESSAGE_BUFFER];
	while(fgets(message, MESSAGE_BUFFER, stdin) != NULL) {
		if (strncmp(message, "/quit", 5) == 0) {
			printf("Closing connection...\n");
			exit(0);
		}
		sendto(new_socket_fd, message, MESSAGE_BUFFER, 0, (struct sockaddr *) &cl_addr, sizeof cl_addr);
	}
}

void * receive(void * socket) {
	int socket_fd, response;
	char message[MESSAGE_BUFFER];
	memset(message, 0, MESSAGE_BUFFER);
	socket_fd = (int) socket;

	while(true) {
		response = recvfrom(socket_fd, message, MESSAGE_BUFFER, 0, NULL, NULL);
		if (response) {
			printf("%s", message);
		}
	}
}


int main(int argc, char**argv) {
	if (argc < 2) {
		printf("Uso: server [puerto] \n");
		exit(1);
	}

	long port = strtol(argv[1], NULL, 10);
	struct sockaddr_in serv, cl_addr;
	int fd, length, response, new_socket_fd;
	int connection;
	char client_address[CLIENT_ADDRESS_LENGTH];
	pthread_t thread;
	char msg[100] = "";
	
	serv.sin_family = AF_INET;
//	serv.sin_port = htons(8096);
	serv.sin_port = port;
	serv.sin_addr.s_addr = INADDR_ANY;
	fd = socket(AF_INET, SOCK_STREAM, 0);

	start_server(fd, &serv);

//	bind(fd, (struct sockaddr *)&sesrv, sizeof(serv));
//	listen(fd, 5); // maximo numero de conexiones

	length = sizeof(cl_addr);
	new_socket_fd = accept(fd, (struct sockaddr *) &cl_addr, &length);
	if (new_socket_fd < 0) {
		printf("Failed to connect\n");
		exit(1);
	}
	
	inet_ntop(AF_INET, &(cl_addr.sin_addr), client_address, CLIENT_ADDRESS_LENGTH);
	printf("Connected: %s\n", client_address);

	// nuevo thread para recibir mensajes
	pthread_create(&thread, NULL, receive, (void *) new_socket_fd);

	// enviar mensaje
	send_message(new_socket_fd, &cl_addr);

	close(new_socket_fd);
	close(fd);
	pthread_exit(NULL);
	return 0;
	/*
	while (connection = accept(fd, (struct sockaddr *)NULL, NULL)) {
		int pid;
		if ((pid = fork()) == 0) {
			while(recv(connection, msg, 100, 0) > 0 ) {
				printf("Message received: %s\n", msg);
//				msg = " ";
			}
			exit(0);
		}
	}
	*/
}
