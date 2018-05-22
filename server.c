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


typedef enum State STATE;

enum State {
    START, CONNECTION, FILENAME, WAIT_DATA, GET_DATA, DONE
};

typedef struct srej Srej;

struct srej {
    int32_t *rejects;
    int total;
};

int check_args(int argc, char * argv[]);
void process_server(int server_sk_num);
void process_client(int32_t server_sk_num, uint8_t *buf, int32_t recv_len, Connection *client);
STATE connection(Connection *client, uint8_t *buf);
STATE filename(Connection *client, int32_t *buf_size, int32_t *window_size, char *fname, uint8_t *buf, int *fd);
STATE wait_data(Connection *client, int32_t buf_size, int32_t window_size, uint8_t *queue, uint8_t *buf, int *fd, Srej *srej);
STATE get_data(Connection *client, int32_t recv_len, uint32_t recv_seq_num, uint8_t *queue, uint8_t *buf, int *fd, Srej *srej, int32_t window_size, int32_t buf_size);
void handle_srej(Connection *client, uint8_t *queue, uint8_t *buf, int *fd, Srej *srej, int32_t window_size, int32_t buf_size, uint32_t *expected_seq, uint32_t *sending_seq);

int main(int argc, char *argv[]) {
    int32_t server_sk_num = 0;
    int port_num = 0;

    port_num = check_args(argc, argv);
    sendtoErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

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
    STATE state = START;
    uint8_t packet[MAX_LEN];
    int fd;
    char *fname;
    int32_t buf_size;
    int32_t window_size;
    int init = 0;
    int i;
    uint8_t *queue;
    Srej srej;
    
    while (state != DONE) {
	switch (state) {
	    case START:
		state = CONNECTION;
		break;
		
	    case CONNECTION:
		state = connection(client, buf);
		break;

 	    case FILENAME:
		state = filename(client, &buf_size, &window_size, fname, buf, &fd);
		break;

	    case WAIT_DATA:
		if (init == 0) {
		    queue = init_queue(buf_size, window_size);
		    srej.rejects = (int32_t *)malloc(window_size*sizeof(int32_t));
		    for (i = 0; i < window_size; i++)
			srej.rejects[i]=0;
		    srej.total = 0;
		    init = 1;
		}
		state = wait_data(client, buf_size, window_size, queue, buf, &fd, &srej);
		break;

	    case DONE:
		break;
	    
	    default:
		printf("you shouldnt be here\n");
		break;
	    }
    }
    if (init == 1)
	free(queue);
    exit(EXIT_SUCCESS);
	
}

STATE connection(Connection *client, uint8_t *buf) {
    uint8_t response[MAX_LEN];
    
    /* create client socket  for responding */
    if ((client->sk_num = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("client socket");
	exit(EXIT_FAILURE);
    }
    send_buf(NULL, 0, client, SETUP_RES, 0, response);
    return FILENAME;
}

STATE filename(Connection *client, int32_t *buf_size, int32_t *window_size, char *fname, uint8_t *buf, int *fd) {
    uint8_t response[MAX_LEN];
    int32_t recv_len;
    uint8_t flag = 0;
    uint32_t seq_num = 0;
    
    if (select_call(client->sk_num, LONG_TIME, 0, NOT_NULL) == 1) {
	recv_len = recv_buf(buf, MAX_LEN, client->sk_num, client, &flag, &seq_num);
	if (recv_len != CRC_ERROR) {
	    if (flag == FNAME) {
		*buf_size = ntohl(*((int32_t*)buf));
		*window_size = ntohl(*((int32_t*)(buf + SIZE_OF_BUF_SIZE)));
	        fname = buf+SIZE_OF_BUF_SIZE*2;
		send_buf(NULL, 0, client, FNAME_RES, 0, response);
	        *fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		return WAIT_DATA;
	    }
	    else
		return CONNECTION;
	}
    }
    else {
	printf("Client appears to have disconnected\n");
	close(client->sk_num);
        return DONE;
    }
    return FILENAME;
}
	
STATE wait_data(Connection *client, int32_t buf_size, int32_t window_size, uint8_t *queue, uint8_t *buf, int *fd, Srej *srej) {
    static int closed = 1;
    uint8_t packet[MAX_LEN];
    int32_t recv_len;
    uint8_t flag = 0;
    uint32_t seq_num = 0;
    if(select_call(client->sk_num, LONG_TIME, 0, NOT_NULL) == 1) {
	recv_len = recv_buf(buf, MAX_LEN, client->sk_num, client, &flag, &seq_num);
	if (recv_len != CRC_ERROR) {
	    if (flag == FNAME) {
		send_buf(NULL, 0, client, FNAME_RES, 0, packet);
		return WAIT_DATA;
	    }
	    if (flag == DATA) {
		return get_data(client, recv_len, seq_num, queue, buf, fd, srej, window_size, buf_size);
	    }
	    else if (flag == EoF) {
		if (closed)
		    close(*fd);
		closed = 0;
		send_buf(NULL, 0, client, EoF, 0, packet);
		return WAIT_DATA;
	    }
	    else if (flag == TERMINATE) { /* client has shutdown */
		close(client->sk_num);
		return DONE;
	    }
	    
	}
	return WAIT_DATA;
    }
    printf("client appears to haver terminated");
    close(*fd);
    close(client->sk_num);
    return DONE;
}

STATE get_data(Connection *client, int32_t recv_len, uint32_t recv_seq_num, uint8_t *queue, uint8_t *buf, int *fd, Srej *srej, int32_t window_size, int32_t buf_size) {
    static uint32_t expected_seq = 1;
    static uint32_t sending_seq = 1;
    int i;
    uint32_t temp_seq;
    uint8_t packet[MAX_LEN];
    /* write to file then send RR 
     * unless there are outstanding SREJs*/
    if(recv_seq_num == expected_seq) { 
	write(*fd, buf, recv_len);
	expected_seq++;
	if (srej->total == 0) { /* no outstanding SREJs, send RR */
	    temp_seq = htonl(expected_seq);
	    safe_memcpy(buf, &temp_seq, SIZE_OF_BUF_SIZE, "write seq to RR");
	    send_buf(buf, SIZE_OF_BUF_SIZE, client, RR, sending_seq++, packet);
	    return WAIT_DATA;
	}
	else {/* there are SREJs, see if buffer can be cleared */
	    handle_srej(client, queue, buf, fd, srej, window_size, buf_size, &expected_seq, &sending_seq);
	    return WAIT_DATA;
	}
    }
    /* recieve a dupe packet
     * discard and send highest RR possible*/
    else if(recv_seq_num < expected_seq) {
	temp_seq = htonl(expected_seq);
	safe_memcpy(buf, &temp_seq, SIZE_OF_BUF_SIZE, "write seq to RR");
	send_buf(buf, SIZE_OF_BUF_SIZE, client, RR, sending_seq++, packet);
	return WAIT_DATA;
    }
    /* recieved out of order packet
     * buffer and send SREJ */
    else if(recv_seq_num > expected_seq) {
	 /* if the packet isnt already buffered */
	if (get_element(queue, recv_seq_num%window_size, buf_size) == NULL) {
	    add_element(queue, recv_seq_num%window_size, buf_size, buf, recv_len);
	    /* if the packet isnt already SREJ'ed */
	    for(i = 0; i < recv_seq_num - expected_seq; i++) { /* sends SREJs */
		if (srej->rejects[(expected_seq+i)%window_size] != expected_seq+i &&
		    get_element(queue, (expected_seq+i)%window_size, buf_size) == NULL) {
		    srej->rejects[(expected_seq+i)%window_size] = expected_seq+i;
		    srej->total++;
		    temp_seq = htonl(expected_seq + i);
		    safe_memcpy(buf, &temp_seq, SIZE_OF_BUF_SIZE, "write seq to SREJ");
		    send_buf(buf, SIZE_OF_BUF_SIZE, client, SREJ, sending_seq++, packet);
		}
	    }
	}
	return WAIT_DATA;
    }

}

void handle_srej(Connection *client, uint8_t *queue, uint8_t *buf, int *fd, Srej *srej, int32_t window_size, int32_t buf_size, uint32_t *expected_seq, uint32_t *sending_seq) {
    uint32_t temp_seq;
    uint8_t packet[MAX_LEN];
    uint8_t *buf_ptr;
    srej->total--;
    srej->rejects[(*expected_seq-1)%window_size] = 0;
    /* clear as much of the buffer as possible */
    while((buf_ptr = (uint8_t *)get_element(queue, *expected_seq%window_size, buf_size)) != NULL) {
	write(*fd, buf_ptr, strlen(buf)); /*might not be strlen */
	remove_element(queue, *expected_seq%window_size, buf_size);
	(*expected_seq)++;
	srej->total--;
	srej->rejects[(*expected_seq-1)%window_size] = 0;
    }
    /* send RR for next unrecieved packet */
    temp_seq = htonl(*expected_seq);
    safe_memcpy(buf, &temp_seq, SIZE_OF_BUF_SIZE, "write seq to RR");
    send_buf(buf, SIZE_OF_BUF_SIZE, client, RR, (*sending_seq)++, packet);
}
