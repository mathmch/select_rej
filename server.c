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

#define DEBUG 1

typedef enum State STATE;

enum State {
    START
};

int check_args(int argc, char * argv[]);
void process_server(int server_sk_num);
void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection *client);


int main(int argc, char *argv[]) {
    int32_t server_sk_num = 0;
    int port_num = 0;

    port_num = check_args(argc, argv);
    if(DEBUG) /*debug mode, no errors */
	sendtoErr_init(atoi(argv[1]), DROP_OFF, FLIP_OFF, DEBUG_ON, RSEED_ON);
    else
	sendtoErr_init(atoi(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

    /* set up main server socket */
    server_sk_num = udp_server(port_num);
    process_server(server_sk_num);
    return 0;
}

int check_args(int argc, char * argv[]){
    int port_num;
    
    if (argc < 2 || argc >3) {
	printf("Usage %s error_rate [port number]\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    else if (argc == 3)
	port_num = atoi(argv[2]);
    else
	port_num = 0;
    return port_num;
}

/* add forking when single instance is working */
void process_server(int server_sk_num) {
    pid_t pid = 0;
    int status = 0;
    uint8_t buf[MAX_LEN];
    Connection client;
    uint8_t flag = 0;
    int32_t seq_num = 0;
    int32_t recv_len = 0;

    while (1) {
	/* waiting on new client */
	if (select_call(server_sk_num, LONG_TIME, 0, NOT_NULL) == 1) {
	    recv_len = recv_buf(buf, MAX_LEN, server_sk_num, &client, &flag, &seq_num);
	    if (recv_len != CRC_ERROR) {
		process_client(server_sk_num, buf, recv_len, &client);
	    }
	}
    }
    
}

void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection *client) {
    printf("Somebody talked to me!\n");

}
