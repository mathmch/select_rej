/* UDP network code - written by Hugh Smith */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "networks.h"
#include "srej.h"

#include "cpe464.h"

/* server setup */
int32_t udp_server(int port_num) {
    int sk = 0;
    struct sockaddr_in local;
    uint32_t len = sizeof(local);
    /* creating the socket */
    if ((sk = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket");
	exit(EXIT_FAILURE);
    }
    /* setting up the socket */
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port_num);
    /* bind the port */
    if (bindMod(sk, (struct sockaddr *)&local, sizeof(local)) < 0) {
	perror("UDP bind");
	exit(EXIT_FAILURE);
    }

    getsockname(sk, (struct sockaddr *)&local, &len);
    printf("Using Port: %d\n", ntohs(local.sin_port));

    return sk;
}

/* set up and save client connection info to server */
int32_t udp_client_setup(char *hostname, uint16_t port_num, Connection *connection) {
    struct hostent *hp = NULL; /* remote host */

    connection->sk_num = 0;
    connection->len = sizeof(struct sockaddr_in);

    /* creating the socket */
    if ((connection->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("cient setup");
	exit(EXIT_FAILURE);
    }

    connection->remote.sin_family = AF_INET;
    hp = gethostbyname(hostname);

    if (hp == NULL) {
	printf("Host no found: %s\n", hostname);
	return -1;
    }

    if (memcpy(&(connection->remote.sin_addr), hp->h_addr, hp->h_length) < 0) {
	perror("memcpy");
	exit(EXIT_FAILURE);
    }

    /* store port of remote */
    connection->remote.sin_port = htons(port_num);

    return 0;
}

int32_t select_call(int32_t socket_num, int32_t seconds, int32_t microseconds, int32_t set_null) {
    fd_set fdvar;
    struct timeval aTimeout;
    struct timeval *timeout = NULL;

    if (set_null == NOT_NULL) {
	aTimeout.tv_sec = seconds;
	aTimeout.tv_usec = microseconds;
	timeout = &aTimeout;
    }

    FD_ZERO(&fdvar);
    FD_SET(socket_num, &fdvar);

    if (select(socket_num+1, &fdvar, NULL, NULL, timeout) < 0) {
	perror("select");
	exit(EXIT_FAILURE);
    }

    if (FD_ISSET(socket_num, &fdvar))
	return 1;
    else
	return 0;
}

int32_t safeSend(uint8_t *packet, uint32_t len, Connection *connection) {
    int send_len = 0;

    if ((send_len == sendtoErr(connection->sk_num, packet, len, 0, (struct sockaddr *)&(connection->remote), connection->len)) < 0) {
	perror("safe send");
	exit(EXIT_FAILURE);
    }
    return send_len;
}

int32_t safeRecv(int recv_sk_num, char *data_buf, int len, Connection *connection) {
    int recv_len = 0;
    uint32_t remote_len = sizeof(struct sockaddr_in);

    if ((recv_len = recvfrom(recv_sk_num, data_buf, len, 0, (struct sockaddr *)&(connection->remote), &remote_len)) < 0) {
	perror("safe recv");
	exit(EXIT_FAILURE);
    }

    connection->len = remote_len;
    return recv_len;

}
