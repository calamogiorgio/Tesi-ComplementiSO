#pragma once
#include <stdint.h>
#include <stddef.h>
#include "tcb.h"
#include "tcb_list.h"

/* Maximum capacity of the message queue (must be a power of 2 for fast masking) */
#define MQ_MAX_SIZE 32  

typedef struct {
    void *buffer[MQ_MAX_SIZE]; // Array of pointers containing the messages
    
    volatile uint8_t head;     // Index for the next element to write
    volatile uint8_t tail;     // Index for the next element to read
    
    TCBList wait_rx_queue;    // Queue for threads blocked waiting to read (queue is empty)
    TCBList wait_tx_queue;    // Queue for threads blocked waiting to write (queue is full)
} MessageQueue;

int8_t msg_queue_init(MessageQueue *m);
int8_t msg_send(MessageQueue *m, void *message);
int8_t msg_receive(MessageQueue *m, void **message);