#pragma once
#include <stdint.h>
#include <stdio.h> 

#define UART_BUFFER_SIZE 32

typedef struct {
    uint8_t buffer[UART_BUFFER_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} RingBuffer;

// Ring Buffer function
void RingBuffer_init(RingBuffer *rb);
uint8_t RingBuffer_push(RingBuffer *rb, uint8_t data);
uint8_t RingBuffer_pop(RingBuffer *rb, uint8_t *data);

// USART hardware functions
void usart_init(uint16_t ubrr);
void usart_putchar(char data);
char usart_getchar(void);
unsigned char usart_kbhit(void);
void usart_pstr(char *s);
void uart_init(void);