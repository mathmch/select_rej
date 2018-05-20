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


uint8_t *init_queue(int32_t buf_size, int32_t window_size) {
    uint8_t *queue = (uint8_t *)malloc(buf_size*window_size);
    return queue;
}

uint8_t *get_element(uint8_t *queue, int index, int32_t buf_size) {
    return queue + index * buf_size;
}

void remove_element(uint8_t *queue, int index, int32_t buf_size) {
    int i;
    for (i = 0; i < buf_size; i++) {
	queue[index*buf_size + i] = 0;
    }
}

int check_index(uint8_t *queue, int index, int32_t buf_size) {
    if(queue[index*buf_size] == 0)
	return 1;
    return 0;
}
