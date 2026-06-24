#pragma once
#include <stdint.h>
#include <stdio.h> // Required for FILE type
#include <message_queue.h>


// USART hardware functions
void usart_init(uint16_t ubrr);
void usart_putchar(char data);
char usart_getchar(void);
unsigned char usart_kbhit(void);
void usart_pstr(char *s);

// Printf redirection functions
int usart_putchar_printf(char var, FILE *stream);
void uart_init(void);