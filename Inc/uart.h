#ifndef UART_H
#define UART_H

#include <stdint.h>

// Name: uart_init
// Description: Initializes USART1 for serial transmission.
// Parameters: None
// Returns: None
void uart_init(void);

// Name: uart_write_string
// Description: Transmits a null-terminated string over UART.
// Parameters: str - Pointer to the string.
// Returns: None
void uart_write_string(const char *str);

// Name: uart_write_uint32
// Description: Transmits a 32-bit unsigned integer as decimal text.
// Parameters: value - Integer to transmit.
// Returns: None
void uart_write_uint32(uint32_t value);

#endif
