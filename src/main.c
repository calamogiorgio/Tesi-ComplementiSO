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

/* Semaphore declarations */
Semaphore sem_p1;
Semaphore sem_p2;

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

/* Thread P1 structures and function */
TCB p1_tcb;
uint8_t p1_stack[THREAD_STACK_SIZE];

void p1_fn(uint32_t arg __attribute__((unused))) {
  while(1) {
    sem_wait(&sem_p1);
    printf("p1\n");
    sem_post(&sem_p2);
  }
}

/* Thread P2 structures and function */
TCB p2_tcb;
uint8_t p2_stack[THREAD_STACK_SIZE];

void p2_fn(uint32_t arg __attribute__((unused))) {
  while(1) {
    sem_wait(&sem_p2);
    printf("p2\n");
    sem_post(&sem_p1);
  }
}

int main(void) {
  /* Initialize UART for printf debugging */
  printf_init();

  /* Initialize semaphores: P1 starts free, P2 starts blocked */
  sem_init(&sem_p1, 1);
  sem_init(&sem_p2, 0);

  /* Create execution contexts for all threads */
  TCB_create(&idle_tcb,
             idle_stack + IDLE_STACK_SIZE - 1,
             idle_fn,
             0);

  TCB_create(&p1_tcb,
             p1_stack + THREAD_STACK_SIZE - 1,
             p1_fn,
             0);

  TCB_create(&p2_tcb,
             p2_stack + THREAD_STACK_SIZE - 1,
             p2_fn,
             0);

  /* Insert threads into the ready queue */
  TCBList_enqueue(&running_queue, &p1_tcb);
  TCBList_enqueue(&running_queue, &p2_tcb);
  TCBList_enqueue(&running_queue, &idle_tcb);

  printf("starting\n");
  
  /* Start the operating system scheduler */
  startSchedule();
}