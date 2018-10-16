#define _GNU_SOURCE
#include <arpa/inet.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>


#include <json.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CONN_NUMBER 10000
#define BUFFER_SIZE 1024

struct client_usr {
	char *id;
	char *name;
	char *status;
};

static pthread_mutex_t msg_lock;
static pthread_mutex_t c_lock;
static int usrcnt = 0;

struct cli_conn {
	char *host;
	char *origin;
	struct client_usr *usr;
	int sfd;
	SLIST_ENTRY(cli_conn) entries;
};
// User lists
static SLIST_HEAD(slisthead, cli_conn) clis_head = SLIST_HEAD_INITIALIZER(clis_head);
/* struct slisthead *clis_headp; */

static void handle_error(char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static void test_set_prop(json_bool *err, json_object *obj, char *key, json_object **dest)
{
	if (*err) {
		return;
	}
	*err = !json_object_object_get_ex(obj, key, dest);
}

static const char* prep_error(const char *msg_s)
{
	json_object *error_j = json_object_new_object();
	json_object *status = json_object_new_string("ERROR");
	json_object *msg = json_object_new_string(msg_s);
	json_object_object_add(error_j, "status", status);
	json_object_object_add(error_j, "message", msg);
	return json_object_to_json_string(error_j);
}

static const char* prep_ok()
{
	char buff[4];
	json_object *error_j = json_object_new_object();
	json_object *status = json_object_new_string("OK");
	json_object *user = json_object_new_object();
	pthread_mutex_lock(&c_lock);
	
	pthread_mutex_unlock(&c_lock);
	/* json_object *usrid = json_object_new_string(); */
	json_object_object_add(error_j, "status", status);
	return json_object_to_json_string(error_j);
}


static void *handle_session(void *data)
{
	char buff[BUFFER_SIZE];
	int bytes_read;
	struct json_object *clientshk_j, *host_j, *origin_j, *user_j;
	struct client_usr *newusr;
	int socketfd = *(int *) data;
	printf("Waiting for handshake...\n");
	bytes_read = read(socketfd, buff, BUFFER_SIZE);
	printf("Handshake: %s\n", buff);
	clientshk_j = json_tokener_parse(buff);
	json_bool error = 0;
	if (!clientshk_j || bytes_read == -1) {
		error = 1;
	}
	test_set_prop(&error, clientshk_j, "host", &host_j);
	test_set_prop(&error, clientshk_j, "origin", &origin_j);
	test_set_prop(&error, clientshk_j, "user", &user_j);
	if (error) {
		const char *error_msg = prep_error("Invalid handshake");
		write(socketfd, error_msg, strlen(error_msg));
		close(socketfd);
		return NULL;
	}
	printf("Handshake approved\n");
	const char *ok_msg = prep_ok();
	write(socketfd, ok_msg, strlen(ok_msg));
	// Enter the event loop
	while (1) {
		bytes_read = read(socketfd, buff, BUFFER_SIZE);
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	int sfd, conn_numbr, nsfd;
	socklen_t client_addr_len;
	struct sockaddr_in server_addr, client_addr;
	if (argc < 2) {
		char buff[50];
		sprintf(buff, "Usage: %s [port]\n", argv[0]);
		handle_error(buff);
	}

	SLIST_INIT(&clis_head);

	if (pthread_mutex_init(&msg_lock, NULL) != 0) { 
	        handle_error("Failed to initialize mutex\n"); 
    	} 

 	if (pthread_mutex_init(&c_lock, NULL) != 0) { 
	        handle_error("Failed to initialize mutex\n"); 
    	}  

	int port = (int) strtol(argv[1], NULL, 10);
	// Create socket fd
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd < 0){
		handle_error("Unable to create socket descriptor");
	}
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(port);
	if (bind(sfd, (struct sockaddr *) &server_addr,
				sizeof(struct sockaddr_in)) == -1) {
		handle_error("Error binding to address");
	}
	if (listen(sfd, 5) == -1) {
		handle_error("Error preparing socket for listening");
	}
	conn_numbr = 0;
	client_addr_len = sizeof(client_addr);
	printf("Socket server initialized at port %d\n", port);
	while (1) {
		nsfd = accept4(sfd,
				(struct sockaddr *) &client_addr,
				&client_addr_len, SOCK_CLOEXEC);
		if (nsfd < 0) {
			handle_error("Error accepting connection");
		}
		conn_numbr++;
		printf("Incoming connection from:\n");
		printf("Address: %d\n", client_addr.sin_addr.s_addr);
		printf("Port: %d\n", client_addr.sin_port);
		printf("socket id: %d\n", nsfd);
		pthread_t thread;
		if (pthread_create(&thread,
					NULL, handle_session,
					&nsfd) != 0) {
			break;
		}
		printf("Pthread Created\n");
		if (pthread_detach(thread) != 0) {
			handle_error("Unable to detach pthread");

		}
	}

	pthread_mutex_destroy(&msg_lock);
	pthread_mutex_destroy(&c_lock);

	/* while (!SLIST_EMPTY(&clis_head)) { */
             /* n1 = SLIST_FIRST(&head); */
             /* SLIST_REMOVE_HEAD(&head, entries); */
             /* free(n1); */
	/* } */

	handle_error("Something failed");
	
	return 0;
}
