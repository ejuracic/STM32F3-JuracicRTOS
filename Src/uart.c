#include <stdint.h>
#include "uart.h"

// Clock control registers
#define RCC_AHBENR    (*(volatile uint32_t *)0x40021014UL)
#define RCC_APB2ENR   (*(volatile uint32_t *)0x40021018UL)

// GPIO Port C registers
#define GPIOC_MODER   (*(volatile uint32_t *)0x48000800UL)
#define GPIOC_OTYPER  (*(volatile uint32_t *)0x48000804UL)
#define GPIOC_AFRL    (*(volatile uint32_t *)0x48000820UL)

// USART1 registers
#define USART1_CR1    (*(volatile uint32_t *)0x40013800UL)
#define USART1_BRR    (*(volatile uint32_t *)0x4001380CUL)
#define USART1_ISR    (*(volatile uint32_t *)0x4001381CUL)
#define USART1_TDR    (*(volatile uint32_t *)0x40013828UL)

// Current clock configuration
#define USART_CLOCK_HZ 8000000UL
#define UART_BAUD_RATE 115200UL

// Name: uart_init
// Description: Initializes USART1 TX on PC4 using alternate
//              function AF7 and a baud rate of 115200.
// Parameters: None
// Returns: None
void uart_init(void)
{
    // Enable GPIOC and USART1 clocks
    RCC_AHBENR |= (1UL << 19);
    RCC_APB2ENR |= (1UL << 14);

    // Read back clock registers before accessing peripherals
    (void)RCC_AHBENR;
    (void)RCC_APB2ENR;

    // Configure PC4 as alternate function mode (10)
    GPIOC_MODER &= ~(3UL << 8);
    GPIOC_MODER |= (2UL << 8);

    // Configure PC4 as push-pull output
    GPIOC_OTYPER &= ~(1UL << 4);

    // Select AF7 (USART1_TX) for PC4
    GPIOC_AFRL &= ~(15UL << 16);
    GPIOC_AFRL |= (7UL << 16);

    // Disable USART before configuring it
    USART1_CR1 = 0U;

    // Configure 115200 baud, oversampling by 16
    USART1_BRR = (USART_CLOCK_HZ + UART_BAUD_RATE / 2U)
                 / UART_BAUD_RATE;

    // Enable transmitter and USART
    USART1_CR1 = (1UL << 3) | (1UL << 0);

    // Wait until transmitter is acknowledged as enabled
    while ((USART1_ISR & (1UL << 21)) == 0U)
    {
    }
}

// Name: uart_write_char
// Description: Waits until USART1 can accept a new byte
//              and transmits one character.
// Parameters: c - Character to transmit.
// Returns: None
static void uart_write_char(char c)
{
    // Wait until transmit data register is empty
    while ((USART1_ISR & (1UL << 7)) == 0U)
    {
    }

    // Send character
    USART1_TDR = (uint8_t)c;
}

// Name: uart_write_string
// Description: Transmits a null-terminated string over USART1.
// Parameters: str - Pointer to the string.
// Returns: None
void uart_write_string(const char *str)
{
    while (*str != '\0')
    {
        uart_write_char(*str);
        str++;
    }
}

// Name: uart_write_uint32
// Description: Converts an unsigned integer to decimal
//              characters and transmits them over USART1.
// Parameters: value - Integer to transmit.
// Returns: None
void uart_write_uint32(uint32_t value)
{
    char digits[10];
    uint32_t count = 0U;

    do
    {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (value != 0U);

    // Send digits in the correct order
    while (count > 0U)
    {
        uart_write_char(digits[--count]);
    }
}


// Name: uart_write_int32
// Description: Transmits a signed 32-bit integer over USART1.
// Parameters: value - Signed integer to transmit.
// Returns: None
void uart_write_int32(int32_t value)
{
    if (value < 0)
    {
        uart_write_string("-");

        // Use int64_t to safely negate the minimum int32_t value
        uart_write_uint32((uint32_t)(-(int64_t)value));
    }
    else
    {
        uart_write_uint32((uint32_t)value);
    }
}

