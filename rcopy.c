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


typedef enum State STATE;

enum State {
    START, CONNECTION, FILENAME, SEND_DATA, DONE
};

typedef struct window Window;

struct window {
    int buf_size;
    int lower;
    int current;
    int upper;
};
    
void check_args(int argc, char *argv[]);
void run_client(int argc, char *argv[]);
STATE start_state(char *argv[], Connection *server);
STATE connection_setup(Connection *server);
STATE filename(char *fname, int32_t buf_size, int32_t window_size, Connection *server);
STATE send_data(int fd, uint8_t *queue, Window *window, Connection *server);

int main(int argc, char *argv[]) {

    check_args(argc, argv);
    sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
    run_client(argc, argv);
    return 0;
}

void run_client(int argc, char *argv[]) {
    Connection server;
    Window window;
    STATE state = START;
    int fd = open(argv[2], O_RDONLY);
    int32_t window_size = atoi(argv[3]);
    int32_t buf_size = atoi(argv[4]);
    uint8_t *queue = (uint8_t*)malloc(window_size*buf_size);
    window.lower = 1;
    window.current = 1;
    window.upper = window_size;
    window.buf_size = buf_size;
    
    while (state != DONE) {
	switch (state){
	case START:
	    state = start_state(argv, &server);
	    break;

	case CONNECTION:
	    state = connection_setup(&server);
	    break;

	case FILENAME:
	    state = filename(argv[2], buf_size, window_size, &server);
	    break;

	case SEND_DATA:
	    state = send_data(fd, queue, &window, &server);
	    break;
	    
	case DONE:
	    close(fd);
	    close(server->sk_num);
	    exit(EXIT_SUCCESS);
	    break;
	    
	default:
	    printf("you shouldnt have gotten here\n");
	    break;
	}
	    
    }
}

STATE start_state(char *argv[], Connection *server) {
    if (server->sk_num > 0)
	close(server->sk_num);
    if (udp_client_setup(argv[6], atoi(argv[7]), server) < 0)
	return DONE;
    return CONNECTION;
}

STATE connection_setup(Connection *server) {
    uint8_t packet[MAX_LEN];
    int32_t seq_num = 0;
    STATE return_state;
    static int retryCount = 0;
    
    send_buf(NULL, 0, server, SETUP, seq_num, packet);
    return processSelect(server, SHORT_TIME, &retryCount, START, FILENAME, DONE);

}

STATE filename(char *fname, int32_t buf_size, int32_t window_size, Connection *server) {
    uint8_t packet[MAX_LEN];
    uint8_t buf[MAX_LEN];
    int fname_len = strlen(fname)+1;
    STATE return_state;
    int32_t recv_len = 0;
    uint8_t flag = 0;
    uint32_t seq_num = 0;
    static int retryCount = 0;
    
    if (retryCount == 0)
	recv_len = recv_buf(buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);
    
    if (recv_len != CRC_ERROR) {
	buf_size = htonl(buf_size);
	window_size = htonl(window_size);
	safe_memcpy(buf, &buf_size, SIZE_OF_BUF_SIZE, "filename");
	safe_memcpy(&buf[SIZE_OF_BUF_SIZE], &window_size, SIZE_OF_BUF_SIZE, "filename");
	safe_memcpy(&buf[SIZE_OF_BUF_SIZE*2], fname, fname_len, "filename");

	send_buf(buf, fname_len + SIZE_OF_BUF_SIZE*2, server, FNAME, 0, packet);
    }
    else if (recv_len == CRC_ERROR)
	     return START;

    return processSelect(server, SHORT_TIME, &retryCount, FILENAME, SEND_DATA, DONE);    
}

STATE send_data(int fd, uint8_t *queue, Window *window, Connection *server) {
    int32_t recv_len;
    uint8_t flag;
    uint32_t seq_num;
    uint8_t packet[MAX_LEN];
    uint8_t buf[MAX_LEN];
    
    if (select_call(server->sk_num, 0, 0, SET_NULL) == 1) {
	recv_len = recv_buf(buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);
	if (flag == FNAME_RES)
	    return SEND_DATA;
    }
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
