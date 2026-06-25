#include "uart.h"
#include "tcb.h"
#include "scheduler.h"
#include "semaphore.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define BAUD 19200
#define MYUBRR F_CPU/16/BAUD-1

// Static ring buffers for transmission and reception
static RingBuffer tx_buffer;
static RingBuffer rx_buffer;

// Semaphores for flow control and thread synchronization
static Semaphore empty_sem_tx;
static Semaphore full_sem_rx;

// ********************************************************************************
// RingBuffer Related
// ********************************************************************************
void RingBuffer_init(RingBuffer *rb) {
    rb->head = 0;
    rb->tail = 0;
}

uint8_t RingBuffer_push(RingBuffer *rb, uint8_t data) {
    // Check if the buffer is full using bitwise mask optimization
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

// ********************************************************************************
// USART Related
// ********************************************************************************
void usart_init(uint16_t ubrr) {
    RingBuffer_init(&tx_buffer);
    RingBuffer_init(&rx_buffer);

    // Initialize semaphores: TX starts free, RX starts empty
    sem_init(&empty_sem_tx, UART_BUFFER_SIZE);
    sem_init(&full_sem_rx, 0);

    // Set baud rate registers
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    
    // Set frame format: 8 data bits, 1 stop bit
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); 
    
    // Enable receiver, transmitter and receive complete interrupt
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0); 
}

void usart_putchar(char data) {
    // Block the thread if there is no free space in the TX buffer
    sem_wait(&empty_sem_tx);
    
    cli();
    RingBuffer_push(&tx_buffer, (uint8_t)data);
    
    // Enable Data Register Empty interrupt to trigger transmission
    UCSR0B |= _BV(UDRIE0);
    sei();
}

char usart_getchar(void) {
    // Block the thread until at least one byte is received
    sem_wait(&full_sem_rx);
    
    cli();
    uint8_t data_extracted = 0;
    RingBuffer_pop(&rx_buffer, &data_extracted);
    sei();
    
    return (char)data_extracted;
}

unsigned char usart_kbhit(void) {
    if (rx_buffer.head == rx_buffer.tail) return 0;
    else return 1;
}

void usart_pstr(char *s) {
    while (*s) { 
        usart_putchar(*s);
        s++;
    }
}
 
int usart_putchar_printf(char var, FILE *stream) {
    if (var == '\n') usart_putchar('\r');
    usart_putchar(var);
    return 0;
}

void uart_init(void){
    usart_init(MYUBRR);
}

// ********************************************************************************
// ISRs
// ********************************************************************************

// USART Data Register Empty ISR
ISR(USART0_UDRE_vect) {
    uint8_t data;
    if (RingBuffer_pop(&tx_buffer, &data) == OK) {
        UDR0 = data;
        // Signal that a slot has been freed up in the TX buffer
        sem_post(&empty_sem_tx);
    } else {
        // Buffer empty, disable the interrupt to prevent endless loops
        UCSR0B &= ~(_BV(UDRIE0));
    }
}

// USART Receive Complete ISR
ISR(USART0_RX_vect) {
    uint8_t data = UDR0;
    
    // Non-blocking write: if the buffer is full, incoming data is safely discarded
    if (RingBuffer_push(&rx_buffer, data) == OK) {
        // Signal that a new byte is available for reading
        sem_post(&full_sem_rx);
    }
}