#ifndef SREJ_H
#define SREJ_H

#include "networks.h"

#define MAX_LEN 1500
#define MAX_TRIES 10
#define LONG_TIME 10
#define SHORT_TIME 1

#pragma pack(1)

typedef struct header Header;

struct header {
    uint32_t seq_num;
    uint16_t checksum;
    uint8_t flag;
};

enum FLAG {
    SETUP = 1, SETUP_RES, DATA, RR = 5, SREJ, FNAME, FNAME_RES, EoF, TERMINATE, CRC_ERROR = -1
};

int32_t send_buf(uint8_t *buf, uint32_t len, Connection *connection, uint8_t flag, uint32_t seq_num, uint8_t *packet);
int createHeader(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t *packet);
int32_t recv_buf(uint8_t *buf, int32_t len, int32_t recv_sk_num, Connection *connection, uint8_t *flag, int32_t *seq_num);
int retrieveHeader(char *data_buf, int recv_len, uint8_t *flag, int32_t *seq_num);
#endif
