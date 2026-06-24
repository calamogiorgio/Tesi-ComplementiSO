#include "uart.h"
#include "tcb.h"
#include "scheduler.h"
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
#define BAUD 19200
#define MYUBRR F_CPU/16/BAUD-1


// ********************************************************************************
// Message Queues Static Instances
// ********************************************************************************
static MessageQueue rx_queue;
static MessageQueue tx_queue;

// ********************************************************************************
// usart Related
// ********************************************************************************
void usart_init( uint16_t ubrr) {

    // Initialize message queues for transmission and reception
    msg_queue_init(&rx_queue);
    msg_queue_init(&tx_queue);

    // Set baud rate
    UBRR0H = (uint8_t)(ubrr>>8);
    UBRR0L = (uint8_t)ubrr;

    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); // 8-bit data
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);   // Enable RX and TX
}
// Transmit a character using the message queue
void usart_putchar(char data) {
    void *message = (void *)(uintptr_t) data;
    
    // Mandiamo il messaggio. msg_send gestisce internamente cli() e sei()
    uint8_t ret = msg_send(&tx_queue, message);
    
    // Se il carattere è stato bufferizzato, abilita l'interrupt hardware
    if (ret == OK) {
        cli();
        UCSR0B |= _BV(UDRIE0);
        sei();
    }
}


// Receive a character from the message queue
char usart_getchar(void) {
    void *message;
    uint8_t ret = msg_receive(&rx_queue, &message);
    // Return the extracted character if available, otherwise return 0
    if (ret == OK) {
        char data_extracted = (char)(uintptr_t) message;
        return data_extracted;
    }
    else return 0;
}

// Interrupt Service Routine for USART0 Data Register Empty
ISR(USART0_UDRE_vect) {
    // Check if tx_queue is not empty
    if (tx_queue.head != tx_queue.tail) {
        // Extract the message and send it to the hardware register
        void* message = tx_queue.buffer[tx_queue.tail];
        char data = (char)(uintptr_t)message;
        UDR0 = data;

        // Increment the tail index FIRST, so the queue effectively has free space now
        tx_queue.tail = (tx_queue.tail + 1) & (MQ_MAX_SIZE - 1);

        // Wake up a blocked writer thread if the queue was full
        if (tx_queue.wait_tx_queue.size > 0) {
            TCB* waken = TCBList_dequeue(&tx_queue.wait_tx_queue);
            waken->status = Ready; // FONDAMENTALE: imposta lo stato a Ready!
            TCBList_enqueue(&running_queue, waken);
        }
    } else {
        // If the queue is empty, disable the Data Register Empty interrupt
        UCSR0B &= ~(_BV(UDRIE0));
    }
}

// Interrupt Service Routine for USART0 Receive Complete
ISR(USART0_RX_vect) {
    // Read the received byte from the hardware data register
    uint8_t data = UDR0;
    
    // Check if the receive queue is NOT full
    if (((rx_queue.head + 1) & (MQ_MAX_SIZE - 1)) != rx_queue.tail) {
        // Cast the byte to a void* pointer to store it in the queue
        void* message = (void*)(uintptr_t)data;
        rx_queue.buffer[rx_queue.head] = message;

        // Wake up a blocked reader thread if any were waiting for data
        if (rx_queue.wait_rx_queue.size > 0) {
            TCB* waken = TCBList_dequeue(&rx_queue.wait_rx_queue);
            waken->status = Ready;
            TCBList_enqueue(&running_queue, waken);
        }

        // Increment the head index using circular masking
        rx_queue.head = (rx_queue.head + 1) & (MQ_MAX_SIZE - 1);
    }
    // If the queue is full, the data byte is discarded :(
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

void uart_init(void){
  // fire up the usart
  usart_init ( MYUBRR );
}