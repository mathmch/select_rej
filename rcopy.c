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
#include "queue.h"

#include "cpe464.h"

/*TODO: refine, figure out why diffs are different */
typedef enum State STATE;

enum State {
    START, CONNECTION, FILENAME, GET_DATA, SEND_DATA, WINDOW, END, DONE
};

typedef struct window Window;

struct window {
    int buf_size;
    int end;
    int size;
    int lower;
    int current;
    int upper;
};
    
void check_args(int argc, char *argv[]);
void run_client(int argc, char *argv[]);
STATE start_state(char *argv[], Connection *server);
STATE connection_setup(Connection *server);
STATE filename(char *fname, int32_t buf_size, int32_t window_size, Connection *server);
STATE get_data(uint8_t *queue, Window *window, Connection *server, char *fname);
STATE send_data(int fd, uint8_t *queue, Window *window, Connection *server);
STATE window_status(uint8_t *queue, Window *window, Connection *server);
STATE terminate(Window *window, Connection *server);


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
    int fd = open(argv[1], O_RDONLY);
    int32_t window_size = atoi(argv[3]);
    int32_t buf_size = atoi(argv[4]);
    uint8_t *queue = init_queue(buf_size, window_size);
    window.size = window_size;
    window.end = 0;
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

	case GET_DATA:
	    state = get_data(queue, &window, &server, argv[2]);
	    break;

	case SEND_DATA:
	    state = send_data(fd, queue, &window, &server);
	    break;

	case WINDOW:
	    state = window_status(queue, &window, &server);
	    break;
	    
	case END:
	    state = terminate(&window, &server);
	    break;
	    
	case DONE:
	    break;
	    
	default:
	    printf("you shouldnt have gotten here\n");
	    break;
	}
	    
    }
    close(fd);
    close(server.sk_num);
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

    return processSelect(server, SHORT_TIME, &retryCount, FILENAME, GET_DATA, DONE);    
}

STATE get_data(uint8_t *queue, Window *window, Connection *server, char *fname) {
    int32_t recv_len;
    uint8_t flag;
    uint32_t seq_num;
    uint32_t rr;
    uint32_t srej;
    uint8_t packet[MAX_LEN];
    uint8_t buf[MAX_LEN];
    uint8_t *buf_ptr;
    
    if (select_call(server->sk_num, 0, 0, NOT_NULL) == 1) {
	recv_len = recv_buf(buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);
	if(recv_len != CRC_ERROR) {
	    if (flag == FNAME_RES) {
		return SEND_DATA;
	    }
	    if (flag == FOPEN_ERR) {
		printf("Error during file open of %s on server\n", fname);
		return DONE;
	    }
	    else if (flag == RR) {
		rr = ntohl(*(int32_t *)buf);
		if (window->end == rr) { /* at eof */
		    window->end = -1; /*indicate RR is up to date */
		    return WINDOW;
		}
		window->upper = window->upper + (rr - window->lower);
		window->lower = rr;
		return GET_DATA;
	    }
	    else if (flag == SREJ) {
		srej = ntohl(*(int32_t *)buf);
		buf_ptr = get_element(queue, srej%window->size, window->buf_size);
		send_buf(buf_ptr, strlen(buf_ptr), server, DATA, srej, packet); /* this may not be MAX_LEN size */
		return GET_DATA;
	    }
	    else if(flag == EoF) { /* tell there server you are terminating */
	        send_buf(NULL, 0, server, TERMINATE, window->current, packet);
		return DONE;
	    }
	}
	else
	    GET_DATA;
    }
    return WINDOW;
}

STATE send_data(int fd, uint8_t *queue, Window *window, Connection *server) {
    int len;
    uint8_t packet[MAX_LEN];
    uint8_t buf[MAX_LEN];
    
    len = read(fd, buf, window->buf_size-HEADER);
    if (len == 0){ /* EOF, need to ensure final RR is recieved */
	window->end = window->current; /* file transfer complete */
	return GET_DATA;
    }
    remove_element(queue, window->current%window->size, window->buf_size);
    add_element(queue, window->current%window->size, window->buf_size, buf, len);
    send_buf(buf, len, server, DATA, window->current++, packet);
    return GET_DATA;
}

STATE window_status(uint8_t *queue, Window *window, Connection *server) {
    uint8_t *buf_ptr;
    static int retryCount = 0;
    uint8_t packet[MAX_LEN];
    if (window->current > window->upper) {
        if (select_call(server->sk_num, SHORT_TIME, 0, NOT_NULL) == 1) {
	    retryCount = 0;
	    return GET_DATA;
	}
	else if (retryCount == 10) { /* window closed and 10 failed attempts */
	    printf("No response from server\n");
	    return DONE;
	}
	else /* window has been closed for 1 sec, send lowest unRRed packet */
	    {
		retryCount++;
		buf_ptr = get_element(queue, window->lower%window->size, window->buf_size);
		send_buf(buf_ptr, strlen(buf_ptr), server, DATA, window->lower, packet);
		return WINDOW;
	    }
    }
    else if (window->end) { /* at eof */
	if (select_call(server->sk_num, SHORT_TIME, 0, NOT_NULL) == 1) {
	    retryCount = 0;
	    return GET_DATA;
	}
	else if(window->end == -1) { /* last RR recieved */
	    retryCount++;
	    send_buf(NULL, 0, server, EoF, window->current++, packet);
	    return WINDOW;
	}
	else { /* outstanding RRs */
	    retryCount++;
	    buf_ptr = get_element(queue, window->lower%window->size, window->buf_size);
	    send_buf(buf_ptr, strlen(buf_ptr), server, DATA, window->lower, packet);
	    return WINDOW;
	}
    }
    else 
	return SEND_DATA;
    }

STATE terminate(Window *window, Connection *server) {
    uint8_t packet[MAX_LEN];
    send_buf(NULL, 0, server, TERMINATE, window->current, packet);
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
