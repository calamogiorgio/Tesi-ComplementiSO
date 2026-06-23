#include "message_queue.h"
#include "scheduler.h"
#include "tcb_list.h"
#include <avr/interrupt.h>

int8_t msg_queue_init(MessageQueue* m) {
    if(m == NULL) return ERROR;

    /* Initialize buffer indexes */
    m->head = 0;
    m->tail = 0;

    /* Initialize block queues */
    m->wait_rx_queue.first = NULL;
    m->wait_rx_queue.last = NULL;
    m->wait_rx_queue.size = 0;

    m->wait_tx_queue.first = NULL;
    m->wait_tx_queue.last = NULL;
    m->wait_tx_queue.size = 0;

    return OK;
}

int8_t msg_send(MessageQueue *m, void *message) {
    if (m == NULL) return ERROR;
    cli();

    /* Enter critical section: block if queue is full */
    while (((m->head + 1) & (MQ_MAX_SIZE - 1)) == m->tail) {
        current_tcb->status = Waiting;
        TCBList_enqueue(&m->wait_tx_queue, current_tcb);
        schedule();
    }

    /* Insert message into the circular buffer */
    m->buffer[m->head] = message;

    /* Wake up a blocked reader if any */
    if(m->wait_rx_queue.size > 0) {
        TCB* waken = TCBList_dequeue(&m->wait_rx_queue);
        TCBList_enqueue(&running_queue, waken);
    }

    /* Update head index */
    m->head = (m->head + 1) & (MQ_MAX_SIZE - 1);
    
    sei();
    return OK;
}

int8_t msg_receive(MessageQueue *m, void **message) {
    if(m == NULL) return ERROR;
    cli();

    /* Enter critical section: block if queue is empty */
    while(m->head == m->tail) {
        current_tcb->status = Waiting;
        TCBList_enqueue(&m->wait_rx_queue, current_tcb);
        schedule();
    }

    /* Extract message from the circular buffer */
    *message = m->buffer[m->tail];

    /* Wake up a blocked writer if any */
    if(m->wait_tx_queue.size > 0) {
        TCB* waken = TCBList_dequeue(&m->wait_tx_queue);
        TCBList_enqueue(&running_queue, waken);
    }

    /* Update tail index */
    m->tail = (m->tail + 1) & (MQ_MAX_SIZE - 1);

    sei();
    return OK;
}