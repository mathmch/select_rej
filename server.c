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

int main(int argc, char *argv[]) {
    int32_t server_sk_num = 0;
    int port_num = 0;

    port_num = check_args(argc, argv);
    if(DEBUG) /*debug mode, no errors */
	sendtoErr_init(atoi(argv[1]), DROP_OFF, FLIP_OFF, DEBUG_ON, RSEED_ON);
    else
	sendtoErr_init(atoi(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
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
