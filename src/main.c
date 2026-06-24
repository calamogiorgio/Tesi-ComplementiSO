#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "tcb.h"
#include "tcb_list.h"
#include "uart.h"
#include "atomport_asm.h"
#include "scheduler.h"
#include "semaphore.h"
#include "message_queue.h"

#define THREAD_STACK_SIZE 256
#define IDLE_STACK_SIZE   128
#define NUM_PROCESSES     20

/* Synchronization and communication resources */
Semaphore sem[NUM_PROCESSES];
MessageQueue my_queue;

/* Thread management structures */
TCB tcb[NUM_PROCESSES];
uint8_t tcb_stack[NUM_PROCESSES][THREAD_STACK_SIZE];

TCB idle_tcb;
uint8_t idle_stack[IDLE_STACK_SIZE];

/**
 * Generic lightweight integer-to-ASCII printer via UART.
 * Replaces the heavy standard library engine and works dynamically
 * regardless of the number of digits.
 */
void my_itoa(uint32_t num) {
    char buf[11]; /* Buffer to hold up to the maximum 32-bit integer + \0 */
    int i = 0;

    /* Handle the zero case explicitly */
    if (num == 0) {
        usart_putchar('0');
        return;
    }

    /* Extract digits from right to left */
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    /* Print the buffer in reverse order to correct the number orientation */
    while (i > 0) {
        usart_putchar(buf[--i]);
    }
}

/**
 * Silent Idle Thread
 * Runs continuously when no other thread is ready.
 */
void idle_fn(uint32_t thread_arg __attribute__((unused))) {
    while(1) {
        asm volatile("nop");
    }
}

/**
 * Producer Thread
 * Waits for its synchronized turn, posts a message, and signals its consumer.
 */
void producer_fn(uint32_t arg) {
    uint8_t id = (uint8_t)arg;
    uint8_t half = NUM_PROCESSES >> 1;
    uint8_t target_consumer = id + half;
    char* msg = "Hello World!";

    if (id == 0) {
        usart_pstr("Kernel Initialized Successfully!\n");
    }

    while(1) {
        /* 1. Wait for producer's turn */
        sem_wait(&sem[id]);

        /* 2. Send the message to the queue */
        msg_send(&my_queue, msg);

        /* 3. Wake up the target consumer */
        sem_post(&sem[target_consumer]);
        
        /* 4. Let the hardware UART breathe and avoid queue saturation */
        _delay_ms(50); 
    }
}

/**
 * Consumer Thread (No printf wrapper used to save stack memory)
 * Fetches the message, prints it sequentially, and triggers the next producer.
 */
void consumer_fn(uint32_t arg) {
    uint8_t id = (uint8_t)arg;
    uint8_t half = NUM_PROCESSES >> 1;
    uint8_t target_producer = id - half; 
    char* received_msg = NULL;

    while(1) {
        /* 1. Wait for the consumer's turn */
        sem_wait(&sem[id]);

        /* 2. Receive the message from the queue */
        msg_receive(&my_queue, (void**)&received_msg);

        /* 3. Atomic and sequential UART output using light custom helper functions */
        usart_pstr("C");
        my_itoa(id);
        usart_pstr(" received from P");
        my_itoa(target_producer);
        usart_pstr(": ");
        usart_pstr(received_msg);
        usart_pstr("\n");

        /* 4. Pass the token back to the next producer in a round-robin fashion */
        uint8_t next_producer = target_producer + 1;
        if (next_producer >= half) {
            next_producer = 0;
        }
        sem_post(&sem[next_producer]);
    }
}

int main(void) {
    uint8_t i;
    uint8_t half = NUM_PROCESSES >> 1;

    /* Initialize the hardware UART peripheral */
    uart_init();

    /* Initialize communication channels */
    msg_queue_init(&my_queue);

    /* Initialize Semaphores: Producer 0 starts unlocked (1), all others locked (0) */
    sem_init(&sem[0], 1);
    for (i = 1; i < NUM_PROCESSES; i++) {
        sem_init(&sem[i], 0);
    }

    /* Instantiate execution context for the Idle Thread */
    TCB_create(&idle_tcb, idle_stack + IDLE_STACK_SIZE - 1, idle_fn, 0);

    /* Instantiate execution contexts for Producers and Consumers */
    for (i = 0; i < NUM_PROCESSES; i++) {
        if (i < half) {
            TCB_create(&tcb[i], &tcb_stack[i][THREAD_STACK_SIZE - 1], producer_fn, i);
        } else {
            TCB_create(&tcb[i], &tcb_stack[i][THREAD_STACK_SIZE - 1], consumer_fn, i);
        }
    }

    /* Populate the Scheduler Ready Queue */
    for (i = 0; i < NUM_PROCESSES; i++) {
        TCBList_enqueue(&running_queue, &tcb[i]);
    }
    TCBList_enqueue(&running_queue, &idle_tcb);

    /* Handover CPU management to the Cooperative Scheduler */
    startSchedule();
    
    return 0;
}