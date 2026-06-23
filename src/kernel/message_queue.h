#pragma once
#include <stdint.h>
#include <stddef.h>
#include "tcb.h"
#include "tcb_list.h"

#define MQ_MAX_SIZE 8  // Anche qui una potenza di 2

typedef struct {
    void *buffer[MQ_MAX_SIZE]; // Array di puntatori a messaggi
    
    volatile uint8_t head;
    volatile uint8_t tail;
    
    TCBList wait_rx_queue;    // Coda dei thread bloccati in lettura (coda vuota)
    TCBList wait_tx_queue;    // Coda dei thread bloccati in scrittura (coda piena)
} MessageQueue;

int8_t msg_queue_init(MessageQueue *m);
int8_t msg_send(MessageQueue *m, void *message);
int8_t msg_receive(MessageQueue *m, void **message);