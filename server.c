#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>

struct sockaddr_in serv;
int fd;
int connection;
char msg[100] = "";

int main() {
	serv.sin_family = AF_INET;
	serv.sin_port = htons(8096);
	server.sin_addr.s_addr = INADDR_ANY;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	bind(fd, (struct sockaddr *)&serv, sizeof(serv));

	listen(fd, 5); // maximo numero de conexiones

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
}
