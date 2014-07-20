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

static FILE *logfp;
static int *listen_ports;
static int *ssockets;
static int nssocket;

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

static void server_loop (void)
{
	fd_set rfds;
	int maxfdp1 = 0, i;

	for (i = 0; i < nssocket; i ++)
		if (maxfdp1 < ssockets[i])
			maxfdp1 = ssockets[i];
	maxfdp1 ++;

	while (1) {
		FD_ZERO(&rfds);
		for (i = 0; i < nssocket; i ++)
			FD_SET(ssockets[i], &rfds);
		int retval = select(maxfdp1, &rfds, NULL, NULL, NULL);

		if (retval == -1) {
			fprintf(logfp, "select() error\n");
			return;
		}
		if (retval == 0) // WTF
			continue;
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
			close(sock);

			char addrstr [64];
			const char *tmp = inet_ntop(cli_addr.sin_family, &(cli_addr.sin_addr), addrstr, sizeof(addrstr));
			assert(tmp != NULL);
			time_t ltime = time(NULL);
			char *timestr = ctime(&ltime);
			if (timestr != NULL && timestr[0] != 0)
				timestr[strlen(timestr) - 1] = '\0';
			fprintf(logfp, "%s\t%d\t%s\t%d\n",
					timestr, listen_ports[i],
					addrstr, ntohs(cli_addr.sin_port));
			fflush(logfp);
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

	fclose(stdin);
	fclose(stdout);
	fclose(stderr);
	change_to_nobody();
	if (fork() != 0) return 0;
	server_loop();
	return 0;
}
