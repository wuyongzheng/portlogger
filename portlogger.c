#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define RECV_TIMEOUT  120
#define MAX_CLIENTNUM 100
#define MAX_DATASIZE  128

struct client_sock_struct {
	int sock;
	int local_port;
	char remote_addr[INET6_ADDRSTRLEN];
	int remote_port;
	time_t conn_time;
	unsigned char recvdata[MAX_DATASIZE];
	int recvsize;
	struct client_sock_struct *prev, *next;
};

static FILE *logfp;
static int *listen_ports;
static int *ssockets;
static int nssocket;
static struct client_sock_struct client_head; // head.next = oldest, head.prev = newest
static int client_num = 0;

static void change_to_nobody (void)
{
	if (geteuid() != 0)
		return;

	struct passwd *nobody = getpwnam("nobody");
	if (nobody == NULL)
		return;

	setgid(nobody->pw_gid);
	setegid(nobody->pw_gid);
	setuid(nobody->pw_uid);
	seteuid(nobody->pw_uid);
}

static struct client_sock_struct *new_client_sock (int sock)
{
	struct client_sock_struct *client_sock = malloc(sizeof(struct client_sock_struct));
	client_sock->sock = sock;
	client_sock->local_port = -1;
	client_sock->remote_port = -1;
	client_sock->remote_addr[0] = '\0';
	client_sock->conn_time = time(NULL);
	memset(client_sock->recvdata, 0, MAX_DATASIZE);
	client_sock->recvsize = 0;
	assert(client_head.prev->next == &client_head);
	client_sock->prev = client_head.prev;
	client_sock->next = &client_head;
	client_head.prev->next = client_sock;
	client_head.prev = client_sock;
	client_num ++;
	return client_sock;
}

static void log_and_delete_client_sock (struct client_sock_struct *client_sock)
{
	assert(client_num > 0);

	if (client_sock->sock != -1) {
		close(client_sock->sock);
		client_sock->sock = -1;
	}

	char *timestr = ctime(&client_sock->conn_time);
	if (timestr != NULL && timestr[0] != 0)
		timestr[strlen(timestr) - 1] = '\0';
	fprintf(logfp, "%s\t%d\t%s\t%d\t%d\t",
			timestr, client_sock->local_port,
			client_sock->remote_addr, client_sock->remote_port,
			client_sock->recvsize);
	int i;
	for (i = 0; i < client_sock->recvsize; i ++) {
		int c = client_sock->recvdata[i];
		if (c == '\\')
			fputs("\\\\", logfp);
		else if (c >= ' ' && c <= '~')
			fputc(c, logfp);
		else if (c == '\r')
			fputs("\\r", logfp);
		else if (c == '\n')
			fputs("\\t", logfp);
		else if (c == '\t')
			fputs("\\t", logfp);
		else if (c == 0)
			fputs("\\0", logfp);
		else
			fprintf(logfp, "\\x%02x", c);
	}
	fputs("\n", logfp);
	fflush(logfp);

	client_sock->prev->next = client_sock->next;
	client_sock->next->prev = client_sock->prev;
	client_num --;
	free(client_sock);
}

static void server_loop (void)
{
	client_head.sock = -1;
	client_head.conn_time = 0;
	client_head.prev = client_head.next = &client_head;

	while (1) {
		fd_set rfds;
		int maxfd = 0, i;

		FD_ZERO(&rfds);
		for (i = 0; i < nssocket; i ++) {
			FD_SET(ssockets[i], &rfds);
			maxfd = ssockets[i] > maxfd ? ssockets[i] : maxfd;
		}
		struct client_sock_struct *client_sock;
		for (client_sock = client_head.next;
				client_sock != &client_head;
				client_sock = client_sock->next) {
			FD_SET(client_sock->sock, &rfds);
			maxfd = client_sock->sock > maxfd ? client_sock->sock : maxfd;
		}
		struct timeval timeout;
		timeout.tv_sec = RECV_TIMEOUT;
		timeout.tv_usec = 0;
		int retval = select(maxfd + 1, &rfds, NULL, NULL, client_num == 0 ? NULL : &timeout);

		if (retval == -1) {
			fprintf(logfp, "select() error\n");
			return;
		}

		if (retval == 0) { // dump all client socks. there should be only one though
			while (client_num > 0)
				log_and_delete_client_sock(client_head.next);
			continue;
		}

		/* do client sockets first. */
		for (client_sock = client_head.next;
				client_sock != &client_head;
				client_sock = client_sock->next) {
			if (!FD_ISSET(client_sock->sock, &rfds))
				continue;
			assert(client_sock->recvsize < MAX_DATASIZE);
			ssize_t recvsize = recv(client_sock->sock,
					client_sock->recvdata + client_sock->recvsize,
					MAX_DATASIZE - client_sock->recvsize, MSG_DONTWAIT);
			if (recvsize > 0 && client_sock->recvsize + recvsize < MAX_DATASIZE) {
				client_sock->recvsize += recvsize;
				continue;
			}
			log_and_delete_client_sock(client_sock);
		}

		/* do server sockets next */
		for (i = 0; i < nssocket; i ++) {
			if (!FD_ISSET(ssockets[i], &rfds))
				continue;
			struct sockaddr_in cli_addr;
			socklen_t clilen = sizeof(cli_addr);
			int sock = accept(ssockets[i], (struct sockaddr *)&cli_addr, &clilen);
			if (sock < 0) {
				fprintf(logfp, "Warning: accept() error\n");
				continue;
			}

			client_sock = new_client_sock(sock);
			client_sock->local_port = listen_ports[i];
			client_sock->remote_port = ntohs(cli_addr.sin_port);
			const char *tmp = inet_ntop(cli_addr.sin_family,
					&(cli_addr.sin_addr),
					client_sock->remote_addr,
					sizeof(client_sock->remote_addr));
			assert(tmp != NULL);

			while (client_num > MAX_CLIENTNUM)
				log_and_delete_client_sock(client_head.next);
		}
	}
}

int main (int argc, char *argv[])
{
	if (argc < 3 || argv[1][0] == '-') {
		printf("Usage: %s logfile port [port ...]\n", argv[0]);
		return 1;
	}

	logfp = fopen(argv[1], "a");
	if (logfp == NULL) {
		perror("can't open log file for appending");
		return 1;
	}

	nssocket = argc - 2;
	listen_ports = malloc(nssocket * sizeof(int));
	ssockets = malloc(nssocket * sizeof(int));
	int i;
	for (i = 0; i < nssocket; i ++) {
		listen_ports[i] = atoi(argv[2+i]);
		if (listen_ports[i] <= 0 || listen_ports[i] >= 65536) {
			fprintf(stderr, "invalid port %s\n", argv[2+i]);
			return 1;
		}

		ssockets[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (ssockets[i] < 0) {
			perror("can't create socket");
			return 1;
		}

		struct sockaddr_in serv_addr;
		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(listen_ports[i]);
		if (bind(ssockets[i], (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
			perror("can't bind to port");
			return 1;
		}
		if (listen(ssockets[i], 5) < 0) {
			perror("can't listen");
			return 1;
		}
	}

	{
		time_t ts = time(NULL);
		char *timestr = ctime(&ts);
		if (timestr != NULL && timestr[0] != 0)
			timestr[strlen(timestr) - 1] = '\0';
		fprintf(logfp, "%s\tListening to:", timestr);
		for (i = 0; i < nssocket; i ++)
			fprintf(logfp, " %d", listen_ports[i]);
		fprintf(logfp, "\n");
		fflush(logfp);
	}

	fclose(stdin);
	fclose(stdout);
	fclose(stderr);
	chdir("/");
	change_to_nobody();
	if (fork() != 0) return 0;
	server_loop();
	return 0;
}
