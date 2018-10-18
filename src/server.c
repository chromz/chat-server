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
	const char *id;
	const char *name;
	const char *status;
};

static pthread_mutex_t glock;
static pthread_mutex_t c_lock;
static int usrcnt = 0;

struct cli_conn {
	const char *host;
	const char *origin;
	struct client_usr *usr;
	int sfd;
	STAILQ_ENTRY(cli_conn) entries;
};
// User lists
static STAILQ_HEAD(slisthead, cli_conn) clis_head = STAILQ_HEAD_INITIALIZER(clis_head);
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

static void print_stailq(void)
{
	struct cli_conn *np;
	STAILQ_FOREACH(np, &clis_head, entries) {
		printf("Client %s\n", np->usr->name);
	}
}

static void alert_all_users(const char *id,
		struct json_object *usr, const char *action_text)
{
	pthread_mutex_lock(&glock);
	struct json_object *usrcnted, *action_j;
	struct cli_conn *current;
	const char *msg;
	usrcnted = json_object_new_object();
	action_j = json_object_new_string(action_text);
	json_object_object_add(usrcnted, "action", action_j);
	json_object_object_add(usrcnted, "user", usr);
	msg = json_object_to_json_string(usrcnted);
	printf("Message: %s\n", msg);
	STAILQ_FOREACH(current, &clis_head, entries) {
		if (strcmp(current->usr->id, id) != 0){
			write(current->sfd, msg, strlen(msg));
		}
	}
	pthread_mutex_unlock(&glock);
}

static const char* prep_ok(int sfd, const char *host,
		const char *origin, const char *username,
		struct cli_conn **usr)
{
	char *id_buff = malloc(sizeof(char) * 1024);
	json_object *data, *status, *user, *id_j, *name, *c_status;
	data = json_object_new_object();
	status = json_object_new_string("OK");
	user = json_object_new_object();
	c_status = json_object_new_string("active");
	name = json_object_new_string(username);
	// Increment usrcnt (thread-safe)
	pthread_mutex_lock(&c_lock);
	sprintf(id_buff, "%d", usrcnt++);
	id_j = json_object_new_string(id_buff);
	pthread_mutex_unlock(&c_lock);
	json_object_object_add(data, "status", status);
	json_object_object_add(user, "id", id_j);
	json_object_object_add(user, "name", name);
	json_object_object_add(user, "status", c_status);
	json_object_object_add(data, "user", user);

	// Add user to user list
	struct cli_conn *new_usr = malloc(sizeof(struct cli_conn));
	new_usr->host = host;
	new_usr->origin = origin;
	new_usr->sfd = sfd;
	new_usr->usr = malloc(sizeof(struct client_usr));
	new_usr->usr->id = id_buff;
	new_usr->usr->name = username;
	new_usr->usr->status = "active";
	alert_all_users(id_buff, user, "USER_CONNECTED");
	pthread_mutex_lock(&glock);
	STAILQ_INSERT_TAIL(&clis_head, new_usr, entries);
	pthread_mutex_unlock(&glock);
	*usr = new_usr;
	
	return json_object_to_json_string(data);
}

static struct json_object *conn_to_usr_json(struct cli_conn *conn)
{
	struct json_object *id_j, *status_j, *name_j;
	struct json_object *user = json_object_new_object();
	id_j = json_object_new_string(conn->usr->id);
	status_j = json_object_new_string(conn->usr->status);
	name_j = json_object_new_string(conn->usr->name);
	json_object_object_add(user, "id", id_j);
	json_object_object_add(user, "name", name_j);
	json_object_object_add(user, "status", status_j);
	return user;

}

static struct cli_conn *find_usr_conn(const char *id)
{
	pthread_mutex_lock(&glock);
	struct cli_conn *current;
	STAILQ_FOREACH(current, &clis_head, entries) {
		if (strcmp(current->usr->id, id) == 0){
			pthread_mutex_unlock(&glock);
			return current;
		}
	}
	pthread_mutex_unlock(&glock);
	return NULL;
}

static const char *handle_list_user(struct json_object *req)
{
	// Create object list
	struct cli_conn *current;
	struct json_object *response, *action_prop, *users, *usr_tmp, *usr_to_search;
	// Check if is only one user
	response = json_object_new_object();
	users = json_object_new_array();
	action_prop = json_object_new_string("LIST_USER");
	int is_in = json_object_object_get_ex(req, "user", &usr_to_search);
	if (!is_in) {
		pthread_mutex_lock(&glock);
		STAILQ_FOREACH(current, &clis_head, entries) {
			usr_tmp = conn_to_usr_json(current);
			json_object_array_add(users, usr_tmp);
		}
		pthread_mutex_unlock(&glock);
		json_object_object_add(response, "action", action_prop);
		json_object_object_add(response, "users", users);
		return json_object_to_json_string(response);
	}
	const char *usr_id = json_object_get_string(usr_to_search);
	struct cli_conn *usr = find_usr_conn(usr_id);
	if (usr == NULL) {
		return prep_error("There is no user with that id");
	}
	json_object_array_add(users, conn_to_usr_json(usr));
	json_object_object_add(response, "action", action_prop);
	json_object_object_add(response, "users", users);
	return json_object_to_json_string(response);

}

static const char *handle_msg(struct json_object *req) {
	// read attributes from request
	struct json_object *req_from, *req_to, *req_msg;
	// TODO: leer data del request

	// attributes to add in response
	struct json_object *response, *action_prop, *from, *to, *message;
	response = json_object_new_object();
	action_prop = json_object_new_string("RECEIVE_MESSAGE");
	from = json_object_new_string("id usuario de");
	to = json_object_new_string("id usuario para");
	message = json_object_new_string("mensajito");

	// insert properties to json
	json_object_object_add(response, "action", action_prop);
	json_object_object_add(response, "from", from);
	json_object_object_add(response, "to", to);
	json_object_object_add(response, "message", message);

	// return built json to string
	return json_object_to_json_string(response);
}

static const char *handle_change_status(struct json_object *req)
{
	struct json_object *response, *status_ok;
	struct json_object *user_j, *status_j;
	json_bool error = 0;
	test_set_prop(&error, req, "user", &user_j);
	test_set_prop(&error, req, "status", &status_j);
	if (error) {
		return prep_error("Invalid action form");
	}
	struct cli_conn *np;
	const char *user_id = json_object_get_string(user_j);
	const char *new_status = json_object_get_string(status_j);
	struct json_object *user_json = NULL;
	STAILQ_FOREACH(np, &clis_head, entries) {
		if (strcmp(user_id, np->usr->id) == 0) {
			np->usr->status = new_status;
			user_json = conn_to_usr_json(np);
		}
	}
	alert_all_users(user_id, user_json, "CHANGED_STATUS");
	response = json_object_new_object();
	status_ok = json_object_new_string("OK");
	json_object_object_add(response, "status", status_ok);
	return json_object_to_json_string(response);
}

static void handle_action(struct json_object *action_j,
		struct json_object *req, int sfd)
{
	const char *action = json_object_get_string(action_j);
	const char *response;
	if (strcmp(action, "LIST_USER") == 0) {
		response = handle_list_user(req);
	} else if (strcmp(action, "SEND_MESSAGE") == 0) {
		response = handle_msg(req);
	} else if (strcmp(action, "CHANGE_STATUS") == 0) {
		response = handle_change_status(req);
	} else {
		response = prep_error("Invalid action");
	}
	printf("MESSAGE %s\n", response);
	write(sfd, response, strlen(response));
}


static void handle_disconnect(struct cli_conn *usr)
{

	struct json_object *usr_j = conn_to_usr_json(usr);
	pthread_mutex_lock(&glock);
	STAILQ_REMOVE(&clis_head, usr, cli_conn, entries);
	free(usr);
	pthread_mutex_unlock(&glock);
	alert_all_users(usr->usr->id, usr_j, "USER_DISCONNECTED");
}

static void *handle_session(void *data)
{
	char buff[BUFFER_SIZE];
	int bytes_read;
	struct json_object *clientshk_j, *host_j, *origin_j, *user_j, *req;
	struct json_object *action_prop;
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
	struct cli_conn *new_usr;
	const char *ok_msg = prep_ok(
			socketfd,
			json_object_get_string(host_j),
			json_object_get_string(origin_j),
			json_object_get_string(user_j), &new_usr);
	write(socketfd, ok_msg, strlen(ok_msg));
	// Enter the event loop
	while (1) {
		bytes_read = read(socketfd, buff, BUFFER_SIZE);
		if (bytes_read == 0) {
			printf("Client disconnected unexpectedly\n");
			handle_disconnect(new_usr);
			close(socketfd);
			return NULL;
		}
		req = json_tokener_parse(buff);
		test_set_prop(&error, req, "action", &action_prop);
		if (error) {
			const char *error_msg = prep_error("Invalid action");
			write(socketfd, error_msg, strlen(error_msg));
			error = 0;
		} else {
			handle_action(action_prop, req, socketfd);
		}
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

	STAILQ_INIT(&clis_head);

	if (pthread_mutex_init(&glock, NULL) != 0) { 
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

	pthread_mutex_destroy(&glock);
	pthread_mutex_destroy(&c_lock);

	/* while (!SLIST_EMPTY(&clis_head)) { */
             /* n1 = SLIST_FIRST(&head); */
             /* SLIST_REMOVE_HEAD(&head, entries); */
             /* free(n1); */
	/* } */

	handle_error("Something failed");
	
	return 0;
}
