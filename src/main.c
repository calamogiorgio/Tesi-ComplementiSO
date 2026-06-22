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

#define THREAD_STACK_SIZE 256
#define IDLE_STACK_SIZE 128
#define NUM_PROCESSES 10

/* Semaphore declarations */
Semaphore sem[NUM_PROCESSES];

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

void process_sequence_loop(uint32_t arg) {
    uint8_t id = (uint8_t)arg;
    uint8_t next_id;
    
    /* Calculate the next process ID in the circular chain */
    if (id == NUM_PROCESSES - 1) {
        next_id = 0;
    } else {
        next_id = id + 1;
    }

    while(1) {
        /* Wait for this process's turn */
        sem_wait(&sem[id]);
        
        printf("p%d\n", id + 1);
        
        /* Wake up the next process in the sequence */
        sem_post(&sem[next_id]);
    }
}


int main(void) {
  uint8_t i;
  /* Initialize UART for printf debugging */
  printf_init();

  /* Initialize semaphores: P1 starts free, the others starts blocked */
  sem_init(&sem[0], 1);

  for (i = 1; i < NUM_PROCESSES; i++) {
    sem_init(&sem[i],0);
  }

  /* Create execution contexts for all threads */

  TCB_create(&idle_tcb,
            idle_stack + IDLE_STACK_SIZE - 1,
            idle_fn,
            0);

  for (i = 0; i < NUM_PROCESSES; i++) {
      TCB_create(&tcb[i],
                &tcb_stack[i][THREAD_STACK_SIZE - 1],
                process_sequence_loop,
                i); /* Passing index as an argument*/
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