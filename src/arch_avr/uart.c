#include "uart.h"
#include "tcb.h"
// ********************************************************************************
// Includes
// ********************************************************************************
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>
// ********************************************************************************
// Macros and Defines
// ********************************************************************************
#define BAUD 19600
#define MYUBRR F_CPU/16/BAUD-1


// ********************************************************************************
// Ring Buffer Static Instances
// ********************************************************************************
static RingBuffer rx_buffer;
static RingBuffer tx_buffer;

// ********************************************************************************
// RingBuffer Related
// ********************************************************************************


void RingBuffer_init(RingBuffer *rb) {
    rb->head = 0;
    rb->tail = 0;
}

uint8_t RingBuffer_push(RingBuffer *rb, uint8_t data) {
    // Check if the buffer is full using an optimized bitwise AND formula
    if (((rb->head + 1) & (UART_BUFFER_SIZE - 1)) == rb->tail) return ERROR;
    
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) & (UART_BUFFER_SIZE - 1);
    return OK;
}

uint8_t RingBuffer_pop(RingBuffer *rb, uint8_t *data) {
    // Check if the buffer is empty
    if (rb->head == rb->tail) return ERROR;

    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) & (UART_BUFFER_SIZE - 1);
    return OK;
}

static FILE mystdout = FDEV_SETUP_STREAM(usart_putchar_printf, NULL, _FDEV_SETUP_WRITE);

// ********************************************************************************
// usart Related
// ********************************************************************************
void usart_init( uint16_t ubrr) {
    // Set baud rate
    UBRR0H = (uint8_t)(ubrr>>8);
    UBRR0L = (uint8_t)ubrr;

    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); // 8-bit data
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);   // Enable RX and TX

    // Initialize software ring buffers for transmission and reception
    RingBuffer_init(&rx_buffer);
    RingBuffer_init(&tx_buffer);
}
// Transmit a character using the ring buffer
void usart_putchar(char data) {
    uint8_t ret = RingBuffer_push(&tx_buffer, data);
    
    // If the character was buffered successfully, enable the Data Register Empty Interrupt
    if (ret == OK) UCSR0B |= _BV(UDRIE0);
}

// Receive a character from the ring buffer
char usart_getchar(void) {
    char data_extracted = 0;
    uint8_t ret = RingBuffer_pop(&rx_buffer, (uint8_t *)&data_extracted);
    
    // Return the extracted character if available, otherwise return 0
    if (ret == OK) return data_extracted;
    else return 0;
}

// Interrupt Service Routine for USART0 Data Register Empty
ISR(USART0_UDRE_vect) {
    char data = 0;
    uint8_t ret = RingBuffer_pop(&tx_buffer, (uint8_t *)&data);
    
    // If there is data in the buffer, send it; otherwise, disable this interrupt
    if (ret == OK) UDR0 = data;
    else UCSR0B &= ~(_BV(UDRIE0));
}

// Interrupt Service Routine for USART0 Receive Complete
ISR(USART0_RX_vect) {
    // Read the received byte from the hardware data register
    uint8_t data = UDR0;
    
    // Push the byte into the software receive buffer
    RingBuffer_push(&rx_buffer, data);
}

unsigned char usart_kbhit(void) {
    if (rx_buffer.head == rx_buffer.tail) return 0;
    else return 1;
}
void usart_pstr(char *s) {
    // loop through entire string
    while (*s) { 
        usart_putchar(*s);
        s++;
    }
}
 
// this function is called by printf as a stream handler
int usart_putchar_printf(char var, FILE *stream) {
    // translate \n to \r for br@y++ terminal
    if (var == '\n') usart_putchar('\r');
    usart_putchar(var);
    return 0;
}

void printf_init(void){
  stdout = &mystdout;
  
  // fire up the usart
  usart_init ( MYUBRR );
}