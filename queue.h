#ifndef QUEUE_H
#define QUEUE_H

uint8_t *init_queue(int32_t buf_size, int32_t window_size);
uint8_t *get_element(uint8_t *queue, int index, int32_t buf_size);
void remove_element(uint8_t *queue, int index, int32_t buf_size);
int check_index(uint8_t *queue, int index, int32_t buf_size);

#endif
