#pragma once

#include <stdint.h>
#include "tcb_list.h"

typedef struct {
    uint8_t value;          
    TCBList wait_queue;
} Semaphore;

int8_t sem_init(Semaphore *sem, uint8_t value);
int8_t sem_wait(Semaphore *sem);
int8_t sem_post(Semaphore *sem);