#include <avr/interrupt.h>
#include <avr/io.h>
#include <assert.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>
#include "tcb.h"
#include "tcb_list.h"
#include "uart.h"
#include "atomport_asm.h"
#include "scheduler.h"
#include "semaphore.h"
#include "message_queue.h"

#define THREAD_STACK_SIZE 256
#define IDLE_STACK_SIZE 128
#define NUM_PROCESSES 10

/* Semaphore declarations */
Semaphore sem[NUM_PROCESSES];

/* Global Message Queue declaration */
MessageQueue my_queue;

/* Thread structures */
TCB tcb[NUM_PROCESSES];
uint8_t tcb_stack[NUM_PROCESSES][THREAD_STACK_SIZE];

/* Idle thread structures and function */
TCB idle_tcb;
uint8_t idle_stack[IDLE_STACK_SIZE];

void idle_fn(uint32_t thread_arg __attribute__((unused))) {
  while(1) {
    cli();
    printf("i\n");
    sei();
    _delay_ms(100);
  }
}

void producer_fn(uint32_t arg) {
    uint8_t id = (uint8_t)arg;
    uint8_t half = NUM_PROCESSES >> 1;
    uint8_t target_consumer = id + half;
    char* msg = "Hello World!";

    while(1) {
        /* 1. Wait for producer's turn */
        sem_wait(&sem[id]);

        /* 2. Send the message to the queue */
        msg_send(&my_queue, msg);

        /* 3. Wake up the target consumer */
        sem_post(&sem[target_consumer]);
        
        _delay_ms(500);
    }
}

void consumer_fn(uint32_t arg) {
    uint8_t id = (uint8_t)arg;
    uint8_t half = NUM_PROCESSES >> 1;
    uint8_t target_producer = id - half; /* The associated producer */
    char* received_msg = NULL;

    while(1) {
        /* 1. Wait for the consumer's turn */
        sem_wait(&sem[id]);

        /* 2. Receive the message from the queue */
        msg_receive(&my_queue, (void**)&received_msg);

        /* 3. Print the result safely */
        cli();
        printf("C%d received from P%d: %s\n", id, target_producer, received_msg);
        sei();

        /* 4. Pass the turn back to the next producer */
        uint8_t next_producer = target_producer + 1;

        if (next_producer == half) {
            next_producer = 0;
        }
        sem_post(&sem[next_producer]);
    }
}


int main(void) {
  uint8_t i;
  /* Initialize UART for printf debugging */
  printf_init();

  /* Initialize semaphores: P1 starts free, the others starts blocked */
  sem_init(&sem[0], 1);

  /* Initialize the message queue */
  msg_queue_init(&my_queue);

  for (i = 1; i < NUM_PROCESSES; i++) {
    sem_init(&sem[i],0);
  }

  /* Create execution contexts for all threads */

  TCB_create(&idle_tcb,
            idle_stack + IDLE_STACK_SIZE - 1,
            idle_fn,
            0);

  uint8_t half = NUM_PROCESSES >> 1;

  for (i = 0; i < NUM_PROCESSES; i++) {
      if (i < half) {
          /* Task creation for Producers */
          TCB_create(&tcb[i], &tcb_stack[i][THREAD_STACK_SIZE - 1], producer_fn, i);
      } else {
          /* Task creation for Consumers */
          TCB_create(&tcb[i], &tcb_stack[i][THREAD_STACK_SIZE - 1], consumer_fn, i);
      }
  }

  /* Insert threads into the ready queue */

  for (i = 0; i < NUM_PROCESSES; i++) {
    TCBList_enqueue(&running_queue, &tcb[i]);
  }

  TCBList_enqueue(&running_queue, &idle_tcb);

  printf("starting\n");
  
  /* Start the operating system scheduler */
  startSchedule();
}
