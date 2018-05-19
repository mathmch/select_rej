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

/* builds and sends the packet */
int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, uint8_t flag, uint32_t seq_num, uint8_t *packet) {
    int32_t sentLen = 0;
    int32_t sendingLen = 0;

    /* build the packet */
    if (len > 0) {
	if(memcpy(&packet[sizeof(Header)], buf, len) < 0) {
	    perror("send buf");
	    exit(EXIT_FAILURE);
	}
    }

    sendingLen = createHeader(len, flag, seq_num, packet);
    sentLen = safeSend(packet, sendingLen, connection);
    return sentLen;
}

/* adds the header to the beginning of the packet */
int createHeader(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t *packet) {
    Header *aHeader = (Header *)packet;
    uint16_t checksum = 0;

    seq_num = htonl(seq_num);
    if (memcpy(&(aHeader->seq_num), &seq_num, sizeof(seq_num)) < 0) {
	perror("create header");
	exit(EXIT_FAILURE);
    }
    aHeader->flag = flag;
    if (memset(&(aHeader->checksum), 0, sizeof(checksum)) < 0) {
	perror("create checksum");
	exit(EXIT_FAILURE);
    }
    checksum = in_cksum((unsigned short*)packet, len + sizeof(Header));
    if (memcpy(&(aHeader->checksum), &checksum, sizeof(checksum)) < 0) {
	perror("write checksum");
	exit(EXIT_FAILURE);
    }

    return len + sizeof(Header);
}

int32_t recv_buf(uint8_t *buf, int32_t len, int32_t recv_sk_num, Connection *connection, uint8_t *flag, int32_t *seq_num) {
    char data_buf[MAX_LEN];
    int32_t recv_len = 0;
    int32_t data_len = 0;

    recv_len = safeRecv(recv_sk_num, data_buf, len, connection);
    data_len = retrieveHeader(data_buf, recv_len, flag, seq_num);

    if (data_len > 0) {
	if (memcpy(buf, &data_buf[sizeof(Header)], data_len) < 0) {
	    perror("recv buf");
	    exit(EXIT_FAILURE);
	}
    }

    return data_len;
}

/* gets the components of the header then returns the length of the data */
int retrieveHeader(char *data_buf, int recv_len, uint8_t *flag, int32_t *seq_num) {
    Header *aHeader = (Header *)data_buf;
    int return_val = 0;

    if (in_cksum((unsigned short *)data_buf, recv_len) != 0) 
	return_val = CRC_ERROR;
    else {
	*flag = aHeader->flag;
	if (memcpy(seq_num, &(aHeader->seq_num), sizeof(aHeader->seq_num)) < 0) {
	    perror("retrieve header");
	    exit(EXIT_FAILURE);
	}
	*seq_num = ntohl(*seq_num);
	return_val = recv_len - sizeof(Header);
    }
    return return_val;
}

/* returns done if function exceeds max tries, timeout if select times out
 * and data ready if there is data */
int processSelect(Connection *connection, int timeout, int *retryCount, int selectTimeoutState, int dataReadyState, int doneState) {
    int return_val = dataReadyState;

    (*retryCount)++;
    if (*retryCount > MAX_TRIES) {
	printf("No response after %d times, other side likely disconnected\n", MAX_TRIES);
	return_val = doneState;
    }
    else {
	if(select_call(connection->sk_num, timeout, 0, NOT_NULL) == 1) {
	    *retryCount = 0;
	    return_val = dataReadyState;
	}
	else {
	    return_val = selectTimeoutState;
	}
    }
    return return_val;
}
