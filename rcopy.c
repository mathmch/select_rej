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
    START, DONE, CONNECTION
};

void check_args(int argc, char *argv[]);
void run_client(int argc, char *argv[]);
STATE start_state(char *argv[], Connection *server);
STATE connection_setup(Connection *server);

int main(int argc, char *argv[]) {

    check_args(argc, argv);
    if(DEBUG) /*debug mode, no errors */
	sendtoErr_init(atoi(argv[1]), DROP_OFF, FLIP_OFF, DEBUG_ON, RSEED_ON);
    else
	sendtoErr_init(atoi(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
    run_client(argc, argv);
    return 0;
}

void run_client(int argc, char *argv[]) {
    Connection server;
    STATE state = START;

    while (state != DONE) {
	switch (state){
	case START:
	    state = start_state(argv, &server);
	    break;

	case CONNECTION:
	    state = connection_setup(&server);
	    
	case DONE:
	    break;
	    
	default:
	    printf("you shouldnt have gotten here\n");
	    break;
	}
	    
    }
}

STATE start_state(char *argv[], Connection *server) {
    if (udp_client_setup(argv[6], atoi(argv[7]), server) < 0)
	return DONE;
    return CONNECTION;
}

STATE connection_setup(Connection *server) {
    uint8_t packet[MAX_LEN];
    int32_t seq_num = 0;
    int sent = 0;
    static int retryCount = 0;
    sent = send_buf(NULL, 0, server, SETUP, seq_num, packet);
    printf("%d\n", sent);
    return DONE;
}

void check_args(int argc, char *argv[]) {
    if (argc != 8) {
	printf("Usage: %s from-filename to-filename window-size buffer-size error-percent remote-machine remote-port\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    if (strlen(argv[1]) > 100) {
	printf("Filename must be less than 100 characters and is: %lu\n", strlen(argv[1]));
	exit(EXIT_FAILURE);
    }
    if (strlen(argv[2]) > 100) {
	printf("Filename must be less than 100 characters and is: %lu\n", strlen(argv[2]));
	exit(EXIT_FAILURE);
    }
    if (atoi(argv[4]) > MAX_LEN) {
	printf("Buffer size must be less than %d and is: %d\n", MAX_LEN, atoi(argv[4]));
	exit(EXIT_FAILURE);
    }
    if (atoi(argv[5]) < 0 || atoi(argv[5]) > 1) {
	printf("Error rate must be between 0 and 1 and is: %d\n", atoi(argv[5]));
	exit(EXIT_FAILURE);
    }
}
